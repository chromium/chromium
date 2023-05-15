// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/metric_reporting_controller.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

constexpr char kSettingPath[] = "path";

class MetricReportingControllerTest : public ::testing::Test {
 public:
  void SetUp() override {
    settings_ = std::make_unique<test::FakeReportingSettings>();
    enable_count_ = 0;
    disable_count_ = 0;
    enable_cb_ = base::BindLambdaForTesting([this]() { ++enable_count_; });
    disable_cb_ = base::BindLambdaForTesting([this]() { ++disable_count_; });
  }

 protected:
  int enable_count_ = 0;
  int disable_count_ = 0;
  base::RepeatingClosure enable_cb_;
  base::RepeatingClosure disable_cb_;

  std::unique_ptr<test::FakeReportingSettings> settings_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(MetricReportingControllerTest, InvalidPath_DefaultDisabled) {
  MetricReportingController controller(settings_.get(), "invalid/path",
                                       /*setting_enabled_default_value=*/false);
  controller.SetSettingUpdateCb(std::move(enable_cb_), std::move(disable_cb_));

  EXPECT_EQ(enable_count_, 0);
  EXPECT_EQ(disable_count_, 0);
}

TEST_F(MetricReportingControllerTest, InvalidPath_DefaultEnabled) {
  MetricReportingController controller(settings_.get(), "invalid/path",
                                       /*setting_enabled_default_value=*/true);
  controller.SetSettingUpdateCb(std::move(enable_cb_), std::move(disable_cb_));

  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 0);
}

TEST_F(MetricReportingControllerTest, TrustedCheck) {
  settings_->SetReportingEnabled(kSettingPath, true);
  settings_->SetIsTrusted(false);

  MetricReportingController controller(settings_.get(), kSettingPath,
                                       /*setting_enabled_default_value=*/false);
  controller.SetSettingUpdateCb(std::move(enable_cb_), std::move(disable_cb_));

  EXPECT_EQ(enable_count_, 0);
  EXPECT_EQ(disable_count_, 0);

  settings_->SetIsTrusted(true);

  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 0);
}

TEST_F(MetricReportingControllerTest, InitiallyEnabled) {
  settings_->SetReportingEnabled(kSettingPath, true);

  MetricReportingController controller(settings_.get(), kSettingPath,
                                       /*setting_enabled_default_value=*/false);
  controller.SetSettingUpdateCb(std::move(enable_cb_), std::move(disable_cb_));

  // Only enable_cb_ is called.
  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 0);

  // Change to disable.
  settings_->SetReportingEnabled(kSettingPath, false);

  // Only disable_cb_ is called.
  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 1);

  // Change to enable.
  settings_->SetReportingEnabled(kSettingPath, true);

  // Only enable_cb_ is called.
  EXPECT_EQ(enable_count_, 2);
  EXPECT_EQ(disable_count_, 1);
}

TEST_F(MetricReportingControllerTest, InitiallyDisabled) {
  settings_->SetReportingEnabled(kSettingPath, false);

  MetricReportingController controller(settings_.get(), kSettingPath,
                                       /*setting_enabled_default_value=*/false);
  controller.SetSettingUpdateCb(std::move(enable_cb_), std::move(disable_cb_));

  // No callbacks are called.
  EXPECT_EQ(enable_count_, 0);
  EXPECT_EQ(disable_count_, 0);

  // Change to enable.
  settings_->SetReportingEnabled(kSettingPath, true);

  // Only enable_cb_ is called.
  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 0);

  // Change to disable.
  settings_->SetReportingEnabled(kSettingPath, false);

  // Only disable_cb_ is called.
  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 1);
}

TEST_F(MetricReportingControllerTest, SetCallbackAfterEnable) {
  settings_->SetReportingEnabled(kSettingPath, false);

  MetricReportingController controller(settings_.get(), kSettingPath,
                                       /*setting_enabled_default_value=*/false);

  // Change to enable.
  settings_->SetReportingEnabled(kSettingPath, true);

  controller.SetSettingUpdateCb(std::move(enable_cb_), std::move(disable_cb_));

  // Only enable_cb_ is called.
  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 0);

  // Change to disable.
  settings_->SetReportingEnabled(kSettingPath, false);

  // Only disable_cb_ is called.
  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 1);
}

TEST_F(MetricReportingControllerTest, SetCallbackAfterDisable) {
  settings_->SetReportingEnabled(kSettingPath, true);

  MetricReportingController controller(settings_.get(), kSettingPath,
                                       /*setting_enabled_default_value=*/false);

  // Change to disable.
  settings_->SetReportingEnabled(kSettingPath, false);

  controller.SetSettingUpdateCb(std::move(enable_cb_), std::move(disable_cb_));

  // No callbacks are called.
  EXPECT_EQ(enable_count_, 0);
  EXPECT_EQ(disable_count_, 0);

  // Change to enable.
  settings_->SetReportingEnabled(kSettingPath, true);

  // Only enable_cb_ is called.
  EXPECT_EQ(enable_count_, 1);
  EXPECT_EQ(disable_count_, 0);
}
}  // namespace
}  // namespace reporting
