// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_drive_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

constexpr char kHypotheticalQueryHistogram[] =
    "Apps.AppList.DriveZeroStateProvider.HypotheticalQuery";

}  // namespace

class ZeroStateDriveProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    provider_ = std::make_unique<ZeroStateDriveProvider>(profile_.get(),
                                                         nullptr, nullptr);
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    provider_->set_session_manager_for_testing(session_manager_.get());
  }

  void FastForwardByMinutes(int minutes) {
    task_environment_.FastForwardBy(base::Minutes(minutes));
  }

  // Check the histogram count and then fast forward in order to bypass the
  // throttling interval.
  void ExpectHistogramCountAndWait(int count) {
    histogram_tester_.ExpectBucketCount(
        kHypotheticalQueryHistogram,
        ZeroStateDriveProvider::ThrottleInterval::kFiveMinutes, count);
    FastForwardByMinutes(5);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ZeroStateDriveProvider> provider_;
  base::HistogramTester histogram_tester_;
};

// Test that each of the trigger events logs a hypothetical query.
TEST_F(ZeroStateDriveProviderTest, HypotheticalQueryTriggers) {
  provider_->OnFileSystemMounted();
  ExpectHistogramCountAndWait(1);

  provider_->ViewClosing();
  ExpectHistogramCountAndWait(2);

  session_manager_->SetSessionState(session_manager::SessionState::ACTIVE);
  provider_->OnSessionStateChanged();
  ExpectHistogramCountAndWait(3);

  power_manager::ScreenIdleState idle_state;
  idle_state.set_dimmed(false);
  idle_state.set_off(false);
  provider_->ScreenIdleStateChanged(idle_state);
  ExpectHistogramCountAndWait(4);
}

// Test that the minimum interval between queries is respected.
TEST_F(ZeroStateDriveProviderTest, HypotheticalQueryIntervals) {
  histogram_tester_.ExpectTotalCount(kHypotheticalQueryHistogram, 0);

  provider_->ViewClosing();
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFiveMinutes, 1);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kTenMinutes, 1);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFifteenMinutes, 1);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kThirtyMinutes, 1);

  FastForwardByMinutes(5);
  provider_->ViewClosing();
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFiveMinutes, 2);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kTenMinutes, 1);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFifteenMinutes, 1);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kThirtyMinutes, 1);

  FastForwardByMinutes(5);
  provider_->ViewClosing();
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFiveMinutes, 3);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kTenMinutes, 2);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFifteenMinutes, 1);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kThirtyMinutes, 1);

  FastForwardByMinutes(5);
  provider_->ViewClosing();
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFiveMinutes, 4);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kTenMinutes, 2);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFifteenMinutes, 2);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kThirtyMinutes, 1);

  FastForwardByMinutes(15);
  provider_->ViewClosing();
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFiveMinutes, 5);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kTenMinutes, 3);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kFifteenMinutes, 3);
  histogram_tester_.ExpectBucketCount(
      kHypotheticalQueryHistogram,
      ZeroStateDriveProvider::ThrottleInterval::kThirtyMinutes, 2);
}

// Test that a hypothetical query is logged when the screen turns on.
TEST_F(ZeroStateDriveProviderTest, HypotheticalQueryOnWake) {
  power_manager::ScreenIdleState idle_state;
  histogram_tester_.ExpectTotalCount(kHypotheticalQueryHistogram, 0);

  // Turn the screen on. This logs a query since the screen state is default off
  // when the provider is initialized.
  idle_state.set_dimmed(false);
  idle_state.set_off(false);
  provider_->ScreenIdleStateChanged(idle_state);
  ExpectHistogramCountAndWait(1);

  // Dim the screen.
  idle_state.set_dimmed(true);
  provider_->ScreenIdleStateChanged(idle_state);
  ExpectHistogramCountAndWait(1);

  // Undim the screen. This should NOT log a query.
  idle_state.set_dimmed(false);
  provider_->ScreenIdleStateChanged(idle_state);
  ExpectHistogramCountAndWait(1);

  // Turn off the screen.
  idle_state.set_dimmed(true);
  idle_state.set_off(true);
  provider_->ScreenIdleStateChanged(idle_state);
  ExpectHistogramCountAndWait(1);

  // Turn on the screen. This logs a query.
  idle_state.set_dimmed(false);
  idle_state.set_off(false);
  provider_->ScreenIdleStateChanged(idle_state);
  ExpectHistogramCountAndWait(2);
}

}  // namespace app_list
