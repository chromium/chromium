// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cloud_upload {

class MetricTest : public testing::Test {
 public:
  MetricTest() = default;

  MetricTest(const MetricTest&) = delete;
  MetricTest& operator=(const MetricTest&) = delete;

 protected:
  enum class TestEnum {
    kZero = 0,
    kOne = 1,
    kMaxValue = kOne,
  };

  Metric<TestEnum> metric_ = Metric<TestEnum>("metric_name");
  base::HistogramTester histogram_;
};

// Tests that Metric::Log() updates the `value` and `state` correctly.
TEST_F(MetricTest, Log) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);
  ASSERT_FALSE(metric_.logged());

  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.value, TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
  ASSERT_TRUE(metric_.logged());

  histogram_.ExpectUniqueSample("metric_name", TestEnum::kOne, 1);

  metric_.Log(TestEnum::kZero);
  ASSERT_EQ(metric_.value, TestEnum::kZero);
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLoggedMultipleTimes);
  ASSERT_TRUE(metric_.logged());

  histogram_.ExpectBucketCount("metric_name", TestEnum::kZero, 1);
}

}  // namespace ash::cloud_upload
