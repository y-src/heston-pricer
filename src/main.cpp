/*  option pricing using the Heston Model.  */

#include <optional>
#include <stdexcept>
#include <string>
#define _USE_MATH_DEFINES

#include "payoffs.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>

void ComputeWeakEulerIteration(double *p_newX, double *p_oldX, double *p_vol,
                               double *p_rv1, double interestRate,
                               double stepLength);

void ComputeNewVol(double *p_vol, double *p_rv1, double *p_rv2,
                   double stepLength, double kappa, double theta, double sigma,
                   double rho);

void GenerateNewRvs(double *p_rv1, double *p_rv2,
                    std::default_random_engine *generator,
                    std::bernoulli_distribution *distribution);

void ComputeError(int noData, double *p_data, double *p_mean, double *p_error);

std::optional<int> get_num_simulations(std::string input) {
  try {
    return std::stoi(input);
  } catch (std::invalid_argument) {
    std::cout << "invalid argument!" << std::endl;
    return {};
  } catch (std::out_of_range) {
    std::cout << "out of range!" << std::endl;
    return {};
  }
}

int main(int argc, char *argv[]) {
  // ensure user provided number of simulations
  if (argc < 2) {
    std::cout << "require number of simulations as argument";
    return 1;
  }

  //  set intial values and coefficients
  int num_simulations;
  if (auto opt_num = get_num_simulations(argv[1])) {
    num_simulations = opt_num.value();
    if (num_simulations < 1) {
      std::cout << "must have positive number of simulations!" << std::endl;
      return 1;
    }
  } else {
    return 1;
  }
  const double start_time = 0.0;
  const double end_time = 1.0;
  const double step_length = 0.0001;
  const int num_steps = (int)((end_time - start_time) / step_length);
  const double initial_price = 100.0;
  const double initial_vol = 0.04;
  const double rate = 0.05;
  const double kappa = 1.2;
  const double theta = 0.04;
  const double sigma = 0.3;
  const double rho = -0.5;
  const double strike_price = 100.0;

  //  allocate memory for pointers
  double *p_oldX = new double;
  double *p_newX = new double;
  double *p_vol = new double;
  double *p_rv1 = new double;
  double *p_rv2 = new double;
  double *p_X = new double[num_simulations];

  //  pointers to LCG random number generator and bernoulli distribution
  std::default_random_engine *p_generator = new std::default_random_engine;
  std::bernoulli_distribution *p_dist = new std::bernoulli_distribution(0.5);

  //  set pointers to starting values
  *p_oldX = initial_price;
  *p_vol = initial_vol;

  /*  Generates approximations for Heston Model (noApproximations) times,
      every approximation is transformed by payoff function, then is
      discounted to get option price at (startT) .    */
  p_generator->seed(12345);
  for (int i = 0; i < num_simulations; i++) {
    //  implement weak Euler-Maruyama scheme for Heston Model
    for (int j = 0; j < num_steps; j++) {
      //  generates bernoulli random variables in [-1.0, 1.0]
      GenerateNewRvs(p_rv1, p_rv2, p_generator, p_dist);
      //  compute one iteration of scheme for heston model
      ComputeWeakEulerIteration(p_newX, p_oldX, p_vol, p_rv1, rate,
                                step_length);
      *p_oldX = *p_newX;
      if (j != num_steps - 1) {
        ComputeNewVol(p_vol, p_rv1, p_rv2, step_length, kappa, theta, sigma,
                      rho);
      }
    }
    //  reset X and vol for next approximation
    *p_oldX = initial_price;
    *p_vol = initial_vol;

    //  price option
    p_X[i] = exp(-rate * (end_time - start_time)) *
             EuropeanPutPayoff(strike_price, *p_newX);
  }

  //  compute mean and standard deviation of MC estimator
  double *p_mean = new double;
  double *p_error = new double;
  ComputeError(num_simulations, p_X, p_mean, p_error);

  std::cout << "Option price at time " << start_time << ": " << *p_mean
            << std::endl
            << "with 99.7% CI : (+/-) "
            << 2.968 * (*p_error) / sqrt(((double)num_simulations)) << std::endl
            << "using h = " << step_length << std::endl
            << "using M = " << num_simulations << std::endl;

  //  deallocate pointers
  delete p_generator;
  delete p_dist;
  delete p_oldX;
  delete p_newX;
  delete p_vol;
  delete p_rv1;
  delete p_rv2;
  delete p_error;
  delete p_mean;
  delete[] p_X;

  return 0;
}

/*  Compute approximate value of X at next step */
void ComputeWeakEulerIteration(double *p_newX, double *p_oldX, double *p_vol,
                               double *p_rv1, double interestRate,
                               double stepLength) {
  *p_newX = *p_oldX + stepLength * interestRate * (*p_oldX) +
            sqrt(stepLength * (*p_vol)) * (*p_oldX) * (*p_rv1);
}

/*  Compute approximate value of Volatility at next step. */
void ComputeNewVol(double *p_vol, double *p_rv1, double *p_rv2,
                   double stepLength, double kappa, double theta, double sigma,
                   double rho) {
  // implement analytic solution through quadratic equation
  double rv = rho * (*p_rv1) + sqrt(1.0 - pow(rho, 2)) * (*p_rv2);
  double a = 1.0 + stepLength * kappa;
  double b = -sqrt(stepLength) * sigma * rv;
  double c =
      -stepLength * kappa * theta - *p_vol + stepLength * (pow(sigma, 2) / 2.0);

  //  check descriminant is non-negative & has one non-negative solution
  double descriminant = pow(b, 2) - 4.0 * a * c;
  assert(descriminant >= pow(b, 2));

  if (descriminant == 0.0) {
    *p_vol = pow(-b / (2.0 * a), 2);
  } else {
    *p_vol = pow((-b + sqrt(descriminant)) / (2.0 * a), 2);
  }
}

/*  Generate two random draws from the bernoulli density.  */
void GenerateNewRvs(double *p_rv1, double *p_rv2,
                    std::default_random_engine *generator,
                    std::bernoulli_distribution *distribution) {
  //  generate bernoulli distributed random variables on [-1.0, 1.0]
  *p_rv1 = (double)(distribution->operator()(*generator) * 2 - 1);
  *p_rv2 = (double)(distribution->operator()(*generator) * 2 - 1);
}

//  Computes mean and standard deviation of given data.
void ComputeError(int noData, double *p_data, double *p_mean, double *p_error) {
  *p_mean = 0.0;
  *p_error = 0.0;
  for (int i = 0; i < noData; i++) {
    *p_mean += p_data[i];
  }
  *p_mean = *p_mean / ((double)noData);
  for (int i = 0; i < noData; i++) {
    *p_error += pow(p_data[i] - *p_mean, 2);
  }
  *p_error = sqrt(*p_error / ((double)noData));
}
