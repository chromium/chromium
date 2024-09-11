// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace {

struct FibonacciState {
  FibonacciState() = default;

  int i{0}, j{1};
};

int ComputeNextFibonacciNumber(FibonacciState* state) {
  int next = state->i + state->j;
  state->i = state->j;
  state->j = next;
  return state->i;
}

// Creates and returns a closure which returns the next Fibonacci number each
// time it is run.
base::RepeatingCallback<int()> MakeFibonacciClosure() {
  auto state = std::make_unique<FibonacciState>();
  return base::BindRepeating(&ComputeNextFibonacciNumber,
                             base::Owned(std::move(state)));
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    LOG(INFO) << argv[0] << ": missing operand";
    return -1;
  }

  int n = 0;
  if (!base::StringToInt(argv[1], &n) || n < 0) {
    LOG(INFO) << argv[0] << ": invalid n '" << argv[1] << "'";
    return -1;
  }

  // `fibonacci_closure1` and `fibonacci_closure2` are independent. Though they
  // are bound to the same method, they each have their own `FibonacciState`.
  // Running one closure does not affect the other.
  base::RepeatingCallback<int()> fibonacci_closure1 = MakeFibonacciClosure();
  base::RepeatingCallback<int()> fibonacci_closure2 = MakeFibonacciClosure();
  for (int i = 0; i < n; ++i) {
    // Run both closures and confirm the values match.
    int fibonacci_i = fibonacci_closure1.Run();
    int fibonacci_i_backup = fibonacci_closure2.Run();
    DCHECK_EQ(fibonacci_i, fibonacci_i_backup);

    LOG(INFO) << "F_" << i << " = " << fibonacci_i;
  }

  return 0;
}
