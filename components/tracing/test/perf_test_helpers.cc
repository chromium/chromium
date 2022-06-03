// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_test_helpers.h"

#include <algorithm>

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace tracing {

namespace {

perf_test::PerfResultReporter SetUpReporter(const std::string& metric) {
  CHECK(::testing::UnitTest::GetInstance() != nullptr) << "Must be GTest.";
  const ::testing::TestInfo* test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  CHECK(test_info != nullptr) << "Must be GTest.";

  perf_test::PerfResultReporter reporter(
      std::string(test_info->test_case_name()) + ".", test_info->name());
  reporter.RegisterImportantMetric(metric, "ms");
  return reporter;
}

}  // namespace

ScopedStopwatch::ScopedStopwatch(const std::string& metric) : metric_(metric) {
  begin_= base::TimeTicks::Now();
}

ScopedStopwatch::~ScopedStopwatch() {
  base::TimeDelta value = base::TimeTicks::Now() - begin_;
  SetUpReporter(metric_).AddResult(metric_, value);
}

IterableStopwatch::IterableStopwatch(const std::string& metric)
    : metric_(metric) {
  begin_ = base::TimeTicks::Now();
}

void IterableStopwatch::NextLap() {
  base::TimeTicks now = base::TimeTicks::Now();
  double elapsed = (now - begin_).InMillisecondsF();
  begin_ = now;
  laps_.push_back(elapsed);
}

IterableStopwatch::~IterableStopwatch() {
  CHECK(!laps_.empty());
  std::sort(laps_.begin(), laps_.end());
  double median = laps_.at(laps_.size() / 2);
  SetUpReporter(metric_).AddResult(metric_, median);
}

}  // namespace tracing
