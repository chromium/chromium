// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/gmock_matchers.h"

#include <memory>
#include <sstream>
#include <string>

#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm::testing {
namespace {

using ::testing::ExplainMatchResult;
using ::testing::Not;
using ::testing::StringMatchResultListener;

template <typename M>
std::string Describe(const M& m) {
  std::stringstream ss;
  m.DescribeTo(&ss);
  return ss.str();
}

template <typename M>
std::string DescribeNegation(const M& m) {
  std::stringstream ss;
  m.DescribeNegationTo(&ss);
  return ss.str();
}

TEST(UkmGmockMatchersTest, HasMetric) {
  base::test::TaskEnvironment task_environment;
  TestAutoSetUkmRecorder ukm_recorder;

  ukm::UkmEntryBuilder builder(ukm::NoURLSourceId(), "Event.Test");
  builder.SetMetric("Metric.Test", 42);
  auto entry = builder.GetEntryForTesting();

  EXPECT_THAT(entry.get(), HasMetric("Metric.Test"));
  EXPECT_THAT(entry.get(), Not(HasMetric("Metric.Missing")));
}

TEST(UkmGmockMatchersTest, HasMetricDescription) {
  auto matcher = HasMetric("Metric.Test");
  EXPECT_EQ("has the UKM metric Metric.Test", Describe(matcher));
  EXPECT_EQ("doesn't have the UKM metric Metric.Test",
            DescribeNegation(matcher));
}

TEST(UkmGmockMatchersTest, HasMetricWithValue) {
  base::test::TaskEnvironment task_environment;
  TestAutoSetUkmRecorder ukm_recorder;

  ukm::UkmEntryBuilder builder(ukm::NoURLSourceId(), "Event.Test");
  builder.SetMetric("Metric.Test", 42);
  auto entry = builder.GetEntryForTesting();

  EXPECT_THAT(entry.get(), HasMetricWithValue("Metric.Test", 42));
  EXPECT_THAT(entry.get(), Not(HasMetricWithValue("Metric.Test", 43)));
  EXPECT_THAT(entry.get(), Not(HasMetricWithValue("Metric.Missing", 42)));
}

TEST(UkmGmockMatchersTest, HasMetricWithValueDescription) {
  auto matcher = HasMetricWithValue("Metric.Test", 42);
  EXPECT_EQ("has the UKM metric Metric.Test with the specific value 42",
            Describe(matcher));
  EXPECT_EQ(
      "doesn't have the UKM metric Metric.Test with the specific value 42",
      DescribeNegation(matcher));
}

TEST(UkmGmockMatchersTest, HasMetricWithValueExplanation) {
  base::test::TaskEnvironment task_environment;
  TestAutoSetUkmRecorder ukm_recorder;

  ukm::UkmEntryBuilder builder(ukm::NoURLSourceId(), "Event.Test");
  builder.SetMetric("Metric.Test", 42);
  auto entry = builder.GetEntryForTesting();

  auto matcher = HasMetricWithValue("Metric.Missing", 42);
  StringMatchResultListener listener;
  EXPECT_FALSE(ExplainMatchResult(matcher, entry.get(), &listener));
  EXPECT_EQ("metric Metric.Missing not found", listener.str());

  auto matcher_wrong_value = HasMetricWithValue("Metric.Test", 43);
  StringMatchResultListener listener_wrong_value;
  EXPECT_FALSE(ExplainMatchResult(matcher_wrong_value, entry.get(),
                                  &listener_wrong_value));
  EXPECT_EQ("metric Metric.Test with incorrect value - got: 42 - expected: 43",
            listener_wrong_value.str());
}

}  // namespace
}  // namespace ukm::testing
