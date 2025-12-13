// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/taskbar_manager.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Because accessing limited access features requires adding a resource
// to the .rc file, and tests don't have .rc files, all we can test
// is that requesting taskbar operations calls the callback with the
// kFeatureNotAvailable failure result.

static constexpr char kShouldPinResultMetric[] =
    "Windows.ShouldPinToTaskbarResult";
static constexpr char kInfobarShouldPinResultMetric[] =
    "Windows.ShouldPinToTaskbarResult.PinToTaskbarInfoBar";
static constexpr char kSettingsShouldPinResultMetric[] =
    "Windows.ShouldPinToTaskbarResult.SettingsPage";
static constexpr char kPinResultMetric[] = "Windows.TaskbarPinResult";
static constexpr char kInfobarPinResultMetric[] =
    "Windows.TaskbarPinResult.PinToTaskbarInfoBar";
static constexpr char kSettingsPinResultMetric[] =
    "Windows.TaskbarPinResult.SettingsPage";

class TaskbarManagerTest : public testing::Test {
 public:
  void OnCanPinToTaskbarResult(bool result) {
    result_ = result;
    got_result_.Quit();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  bool result_ = false;
  base::RunLoop got_result_;
  base::HistogramTester histogram_tester_;
};

TEST_F(TaskbarManagerTest, ShouldOfferToPin) {
  browser_util::ShouldOfferToPin(
      ShellUtil::GetBrowserModelId(/*is_per_user_install=*/true),
      browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
      base::BindOnce(&TaskbarManagerTest::OnCanPinToTaskbarResult,
                     base::Unretained(this)));

  got_result_.Run();
  EXPECT_FALSE(result_);
  histogram_tester_.ExpectBucketCount(
      kShouldPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable, 1);
  histogram_tester_.ExpectBucketCount(
      kInfobarShouldPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable, 1);
  histogram_tester_.ExpectBucketCount(
      kSettingsShouldPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable, 0);
}

TEST_F(TaskbarManagerTest, PinToTaskbar) {
  browser_util::PinAppToTaskbar(
      ShellUtil::GetBrowserModelId(/*is_per_user_install=*/true),
      browser_util::PinAppToTaskbarChannel::kSettingsPage,
      base::BindOnce(&TaskbarManagerTest::OnCanPinToTaskbarResult,
                     base::Unretained(this)));

  got_result_.Run();
  histogram_tester_.ExpectBucketCount(
      kPinResultMetric, browser_util::PinResultMetric::kFeatureNotAvailable, 1);
  histogram_tester_.ExpectBucketCount(
      kInfobarPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable, 0);
  histogram_tester_.ExpectBucketCount(
      kSettingsPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable, 1);
  EXPECT_FALSE(result_);
}
