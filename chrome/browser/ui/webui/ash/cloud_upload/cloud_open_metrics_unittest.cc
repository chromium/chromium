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
    kTwo = 2,
    kMaxValue = kTwo,
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

// Tests that Metric::ExpectNotLogged() updates the `state` correctly when the
// metric was never logged.
TEST_F(MetricTest, ExpectNotLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.ExpectNotLogged();
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);
}

// Tests that Metric::ExpectNotLogged() updates the `state` correctly when the
// metric was logged.
TEST_F(MetricTest, ExpectNotLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.ExpectNotLogged();
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLogged);
}

// Tests that Metric::ExpectLogged() updates the `state` correctly when the
// metric was logged.
TEST_F(MetricTest, ExpectLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.ExpectLogged();
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests that Metric::ExpectLogged() updates the `state` correctly when the
// metric was never logged.
TEST_F(MetricTest, ExpectLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.ExpectLogged();
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

// Tests that Metric::ExpectLoggedWith() updates the `state` correctly when
// logged with the correct value.
TEST_F(MetricTest, ExpectLoggedWithWhenLoggedWithCorrectValue) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.ExpectLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests Metric::ExpectLoggedWith()  updates the `state` correctly when not
// logged.
TEST_F(MetricTest, ExpectLoggedWithWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.ExpectLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

// Tests that Metric::ExpectLoggedWith() updates the `state` correctly when
// logged with the wrong value.
TEST_F(MetricTest, ExpectLoggedWithWhenLoggedWithWrongValue) {
  metric_.Log(TestEnum::kTwo);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.ExpectLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kWrongValueLogged);
}

}  // namespace ash::cloud_upload
