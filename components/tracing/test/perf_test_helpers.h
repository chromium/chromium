// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_TEST_PERF_TEST_HELPERS_H_
#define COMPONENTS_TRACING_TEST_PERF_TEST_HELPERS_H_

#include <stdint.h>

#include <vector>

#include "base/time/time.h"

namespace tracing {

// Measure time spent in a scope.
// Must be used inside GTest case and result will be printed in perf_test format
// with value name passed to constructor.
class ScopedStopwatch {
 public:
  ScopedStopwatch(const std::string& metric);

  ScopedStopwatch(const ScopedStopwatch&) = delete;
  ScopedStopwatch& operator=(const ScopedStopwatch&) = delete;

  ~ScopedStopwatch();

 private:
  base::TimeTicks begin_;
  const std::string metric_;
};

// Measure median time of loop iterations.
// Example:
//   IterableStopwatch stopwatch("foo");
//   for (int i = 0; i < 100; i++) {
//     ...
//     stopwatch.NextLap();
//   }
// Must be used inside GTest case and result will be printed in perf_test format
// with value name passed to constructor.
class IterableStopwatch {
 public:
  IterableStopwatch(const std::string& metric);

  IterableStopwatch(const IterableStopwatch&) = delete;
  IterableStopwatch& operator=(const IterableStopwatch&) = delete;

  ~IterableStopwatch();

  void NextLap();

 private:
  base::TimeTicks begin_;
  std::vector<double> laps_;
  const std::string metric_;
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_TEST_PERF_TEST_HELPERS_H_
