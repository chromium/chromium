// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_rate_controller.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

class MetricRateControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    settings_ = std::make_unique<test::FakeReportingSettings>();
    run_count_ = 0;
    run_cb_ = base::BindLambdaForTesting([this]() { ++run_count_; });
  }

 protected:
  const std::string rate_setting_path_ = "rate_path";

  std::unique_ptr<test::FakeReportingSettings> settings_;

  int run_count_ = 0;
  base::RepeatingClosure run_cb_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(MetricRateControllerTest, InvalidPath) {
  MetricRateController controller(run_cb_, settings_.get(), "invalid_path",
                                  /*default_rate=*/base::Seconds(5));

  controller.Start();

  EXPECT_EQ(run_count_, 0);

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_EQ(run_count_, 1);
}

TEST_F(MetricRateControllerTest, TrustedCheck) {
  // Set rate setting to 7000 milliseconds.
  settings_->SetInteger(rate_setting_path_, 7000);
  settings_->SetIsTrusted(false);

  MetricRateController controller(run_cb_, settings_.get(), rate_setting_path_,
                                  /*default_rate=*/base::Milliseconds(5000));

  controller.Start();

  EXPECT_EQ(run_count_, 0);

  task_environment_.FastForwardBy(base::Milliseconds(5000));
  EXPECT_EQ(run_count_, 1);

  // Values are trusted, this won't get picked up until the next run.
  settings_->SetIsTrusted(true);

  task_environment_.FastForwardBy(base::Milliseconds(5000));
  EXPECT_EQ(run_count_, 2);

  // Now rate setting should be used since the values are trusted.
  // No new runs expected after 5000 milliseconds.
  task_environment_.FastForwardBy(base::Milliseconds(5000));
  EXPECT_EQ(run_count_, 2);

  // A new run is expected after 7000 milliseconds (5000 + 2000).
  task_environment_.FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(run_count_, 3);
}

TEST_F(MetricRateControllerTest, BaseRateSetting) {
  // Set rate setting to 10 seconds.
  settings_->SetInteger(rate_setting_path_, 10);

  MetricRateController controller(run_cb_, settings_.get(), rate_setting_path_,
                                  /*default_rate=*/base::Seconds(5),
                                  /*rate_unit_to_ms=*/1000);

  // Start not called, no runs expected.
  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_EQ(run_count_, 0);

  controller.Start();

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_EQ(run_count_, 0);

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_EQ(run_count_, 1);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(run_count_, 2);

  controller.Stop();
  // Controller stopped so new runs.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(run_count_, 2);
}

TEST_F(MetricRateControllerTest, UpdateRateSetting) {
  // Set rate setting to 9000 milliseconds.
  settings_->SetInteger(rate_setting_path_, 9000);

  MetricRateController controller(run_cb_, settings_.get(), rate_setting_path_,
                                  /*default_rate=*/base::Milliseconds(5000));

  controller.Start();

  task_environment_.FastForwardBy(base::Milliseconds(9000));
  EXPECT_EQ(run_count_, 1);

  // Update rate setting, it won't get picked up until the next run.
  settings_->SetInteger(rate_setting_path_, 2000);

  task_environment_.FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(run_count_, 1);
  task_environment_.FastForwardBy(base::Milliseconds(7000));
  EXPECT_EQ(run_count_, 2);

  task_environment_.FastForwardBy(base::Milliseconds(2000));
  EXPECT_EQ(run_count_, 3);
}

TEST_F(MetricRateControllerTest, RateSettingZero) {
  // Set rate setting to 0.
  settings_->SetInteger(rate_setting_path_, 0);

  MetricRateController controller(run_cb_, settings_.get(), rate_setting_path_,
                                  /*default_rate=*/base::Milliseconds(5000));

  controller.Start();

  // Fallback to default rate.
  task_environment_.FastForwardBy(base::Milliseconds(5000));
  EXPECT_EQ(run_count_, 1);
}
}  // namespace
}  // namespace reporting
