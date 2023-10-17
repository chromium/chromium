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

// Tests that Metric::MakeInconsistentIfLogged() doesn't update the `state` when
// the metric was not logged.
TEST_F(MetricTest, MakeInconsistentIfLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.MakeInconsistentIfLogged();
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);
}

// Tests that Metric::MakeInconsistentIfLogged() updates the `state` correctly
// when the metric was logged.
TEST_F(MetricTest, MakeInconsistentIfLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfLogged();
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLogged);
}

// Tests that Metric::MakeInconsistentIfNotLogged() doesn't update the `state`
// when the metric was logged.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfNotLogged();
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests that Metric::MakeInconsistentIfNotLogged() updates the `state`
// correctly when the metric was not logged.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.MakeInconsistentIfNotLogged();
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

// Tests that Metric::MakeInconsistentIfNotLoggedWith() doesn't update the
// `state` when logged with the correct value.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWithWhenLoggedWithCorrectValue) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfNotLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests Metric::MakeInconsistentIfNotLoggedWith() updates the `state` correctly
// when not logged.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWithWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.MakeInconsistentIfNotLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

// Tests that Metric::MakeInconsistentIfNotLoggedWith() updates the `state`
// correctly when logged with the wrong value.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWithWhenLoggedWithWrongValue) {
  metric_.Log(TestEnum::kTwo);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfNotLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kWrongValueLogged);
}

}  // namespace ash::cloud_upload
