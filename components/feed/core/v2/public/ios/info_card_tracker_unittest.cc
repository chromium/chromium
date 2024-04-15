// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/ios/info_card_tracker.h"

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#import "components/feed/core/proto/v2/wire/info_card.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ios_feed {
namespace {

// Test values for the tracking state.
const int info_card_type = 100;
const int multiple_trigger_count = 3;
const int view_fraction_threshold = 2;
const int minimum_seconds_between_views = 1;

class IOSInfoCardTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    feed::RegisterProfilePrefs(browser_state_prefs_.registry());
  }

 protected:
  TestingPrefServiceSimple browser_state_prefs_;
};

// Test a tracking command that triggers an explicit dismissal.
TEST_F(IOSInfoCardTrackerTest, OnExplicitDismissal) {
  InfoCardTracker tracker(&browser_state_prefs_);

  // Get the info card tracking state, which should be defaulted with 0 explicit
  // dismissmals.
  feedwire::InfoCardTrackingState tracking_state =
      tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.explicitly_dismissed_count(), 0);

  // Trigger an explicit dismissal and check that the count has incremented
  // once.
  tracker.OnTrackInfoCardCommand(
      info_card_type, (int)TrackingType::kExplicitDismissal,
      view_fraction_threshold, minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.explicitly_dismissed_count(), 1);

  // Trigger multiple explicit dismissals and check that the count has
  // incremented the correct amount of times.
  for (int i = 0; i < multiple_trigger_count; ++i) {
    tracker.OnTrackInfoCardCommand(
        info_card_type, (int)TrackingType::kExplicitDismissal,
        view_fraction_threshold, minimum_seconds_between_views);
  }
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.explicitly_dismissed_count(),
            1 + multiple_trigger_count);

  // Trigger a different command and check that the explicit dismissal count
  // didn't change.
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kClick,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.explicitly_dismissed_count(),
            1 + multiple_trigger_count);
}

// Test a tracking command that triggers a view.
TEST_F(IOSInfoCardTrackerTest, OnView) {
  InfoCardTracker tracker(&browser_state_prefs_);

  // Get the info card tracking state, which should be defaulted with 0 views.
  feedwire::InfoCardTrackingState tracking_state =
      tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.view_count(), 0);

  // Trigger a view and check that the count has incremented once.
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kView,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.view_count(), 1);

  // Trigger another view immediately and check that it didn't increment since
  // it hasn't been `minimum_seconds_between_views`.
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kView,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.view_count(), 1);

  // Trigger another view after `minimum_seconds_between_views` and check that
  // the count incremented.
  base::PlatformThread::Sleep(base::Seconds(minimum_seconds_between_views));
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kView,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.view_count(), 2);

  // Trigger a different command and check that the view count didn't change.
  tracker.OnTrackInfoCardCommand(
      info_card_type, (int)TrackingType::kExplicitDismissal,
      view_fraction_threshold, minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.view_count(), 2);

  // Trigger a ReportView command after `minimum_seconds_between_views` which
  // should be handled the same way as a view, incrementing the view count.
  base::PlatformThread::Sleep(base::Seconds(minimum_seconds_between_views));
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kReportView,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.view_count(), 3);
}

// Test a tracking command that triggers a click.
TEST_F(IOSInfoCardTrackerTest, OnClick) {
  InfoCardTracker tracker(&browser_state_prefs_);

  // Get the info card tracking state, which should be defaulted with 0 clicks.
  feedwire::InfoCardTrackingState tracking_state =
      tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.click_count(), 0);

  // Trigger a click and check that the count has incremented once.
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kClick,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.click_count(), 1);

  // Trigger multiple clicks and check that the count has incremented the
  // correct amount of times.
  for (int i = 0; i < multiple_trigger_count; ++i) {
    tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kClick,
                                   view_fraction_threshold,
                                   minimum_seconds_between_views);
  }
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.click_count(), 1 + multiple_trigger_count);

  // Trigger a different command and check that the click count didn't change.
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kView,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracking_state = tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(tracking_state.click_count(), 1 + multiple_trigger_count);
}

// Test a tracking command that resets the tracking state.
TEST_F(IOSInfoCardTrackerTest, OnResetState) {
  InfoCardTracker tracker(&browser_state_prefs_);

  // Store the default tracking state to later compare after resetting the
  // state.
  feedwire::InfoCardTrackingState default_tracking_state =
      tracker.GetInfoCardTrackingStateFromPref(info_card_type);

  // Trigger a view, an explicit dismissal, and multiple clicks.
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kView,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  tracker.OnTrackInfoCardCommand(
      info_card_type, (int)TrackingType::kExplicitDismissal,
      view_fraction_threshold, minimum_seconds_between_views);
  for (int i = 0; i < multiple_trigger_count; ++i) {
    tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kClick,
                                   view_fraction_threshold,
                                   minimum_seconds_between_views);
  }

  // Check that the current tracking state reflects the commands.
  feedwire::InfoCardTrackingState current_tracking_state =
      tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(current_tracking_state.view_count(), 1);
  EXPECT_EQ(current_tracking_state.explicitly_dismissed_count(), 1);
  EXPECT_EQ(current_tracking_state.click_count(), multiple_trigger_count);

  // Trigger ResetState and check that the current tracking state has been reset
  // to the default tracking state.
  tracker.OnTrackInfoCardCommand(info_card_type, (int)TrackingType::kResetState,
                                 view_fraction_threshold,
                                 minimum_seconds_between_views);
  current_tracking_state =
      tracker.GetInfoCardTrackingStateFromPref(info_card_type);
  EXPECT_EQ(current_tracking_state.view_count(),
            default_tracking_state.view_count());
  EXPECT_EQ(current_tracking_state.explicitly_dismissed_count(),
            default_tracking_state.explicitly_dismissed_count());
  EXPECT_EQ(current_tracking_state.click_count(),
            default_tracking_state.click_count());
}

}  // namespace
}  // namespace ios_feed
