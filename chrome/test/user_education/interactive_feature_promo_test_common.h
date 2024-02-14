// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_COMMON_H_
#define CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_COMMON_H_

#include <variant>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

// Base class holding common definitions used across the
// InteractiveFeaturePromoTest* classes.
class InteractiveFeaturePromoTestCommon {
 public:
  // The type of mock tracker that is used when `UseMockTracker` is specified.
  using MockTracker = testing::NiceMock<feature_engagement::test::MockTracker>;

  // Indicates that a mock `FeatureEngagementTracker` should be used.
  struct UseMockTracker {};

  // Specified when the normal `FeatureEngagementTracker` should be used and a
  // specific set of feature promos should be enabled.
  using UseDefaultTrackerAllowingPromos = std::vector<base::test::FeatureRef>;

  // Allows either a mock tracker or a default tracker with specific promos
  // to be used.
  using TrackerMode =
      std::variant<UseMockTracker, UseDefaultTrackerAllowingPromos>;

  // Describes which clock the User Education system should use to determine
  // things like session duration, snooze time, cooldowns, etc.
  enum class ClockMode {
    // Use the default clock - typically the system clock unless it has already
    // been overridden for testing.
    kUseDefaultClock,
    // Use a specific test clock owned by this test object. Time can be advanced
    // manually.
    kUseTestClock
  };

  // Descrbies how the test should start. You can use `session_test_util()` to
  // further update the session state.
  enum InitialSessionState {
    // The browser has been idle for a while; most promos will not be shown.
    kIdle,
    // The browser has started recently and only lightweight and high-priority
    // promos may be shown.
    kInsideGracePeriod,
    // The browser started a while ago and all promos are eligible to show if
    // they meet other requirements.
    kOutsideGracePeriod
  };

  // Specifies a new time to update to when using the test clock.
  using NewTime = std::variant<
      // An absolute time; should be later than the previous time specified.
      base::Time,
      // A relative time based on the current test time.
      base::TimeDelta>;
};

#endif  // CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_COMMON_H_
