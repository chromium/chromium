// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumbs_status.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace breadcrumbs {

class BreadcrumbsStatusTest : public testing::Test {
 public:
  void SetUp() override {
    breadcrumbs::RegisterPrefs(prefs_.registry());
    ASSERT_FALSE(breadcrumbs::IsEnabled(prefs()));
    testing::Test::SetUp();
  }

 protected:
  PrefService* prefs() { return &prefs_; }

  void SetBreadcrumbsEnabledPrefs(bool is_enabled, base::Time enabled_time) {
    prefs()->SetBoolean(breadcrumbs::kEnabledPref, is_enabled);
    prefs()->SetTime(breadcrumbs::kEnabledTimePref, enabled_time);
  }

  // Asserts via EXPECT_* that breadcrumbs' status is `expected_is_enabled`, and
  // that its "enabled" and "enabled_time" prefs are set to
  // `expected_is_enabled` and `expected_enabled_time`, respectively.
  void ExpectBreadcrumbsStatusIs(bool expected_is_enabled,
                                 base::Time expected_enabled_time) {
    EXPECT_EQ(expected_is_enabled, breadcrumbs::IsEnabled(prefs()));
    EXPECT_EQ(expected_is_enabled,
              prefs()->GetBoolean(breadcrumbs::kEnabledPref));
    EXPECT_EQ(expected_enabled_time,
              prefs()->GetTime(breadcrumbs::kEnabledTimePref));
  }

 private:
  TestingPrefServiceSimple prefs_;

  // Mock time to allow precise expectations about timestamps.
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the ScopedEnableBreadcrumbsForTesting object enables breadcrumbs
// while it is in scope, and returns it to its default state when deleted.
TEST_F(BreadcrumbsStatusTest, ScopedEnableForTesting) {
  {
    ScopedEnableBreadcrumbsForTesting scoped_breadcrumbs_enabled_;
    EXPECT_TRUE(breadcrumbs::IsEnabled(prefs()));
  }
  EXPECT_FALSE(breadcrumbs::IsEnabled(prefs()));
}

// Tests that if breadcrumbs is enabled in user prefs and the prefs' timestamp
// is current, breadcrumbs will be enabled without changing the prefs.
TEST_F(BreadcrumbsStatusTest, EnabledFromPrefs) {
  const auto enabled_time = base::Time::Now() - base::Minutes(1);
  SetBreadcrumbsEnabledPrefs(/*is_enabled=*/true, enabled_time);
  breadcrumbs::MaybeEnableBasedOnChannel(prefs(),
                                         version_info::Channel::UNKNOWN);
  EXPECT_NO_FATAL_FAILURE(ExpectBreadcrumbsStatusIs(true, enabled_time));
}

// Tests that if breadcrumbs is disabled in user prefs and the prefs' timestamp
// is current, breadcrumbs will be disabled without changing the prefs.
TEST_F(BreadcrumbsStatusTest, DisabledFromPrefs) {
  const auto disabled_time = base::Time::Now() - base::Minutes(1);
  SetBreadcrumbsEnabledPrefs(/*is_enabled=*/false, disabled_time);
  breadcrumbs::MaybeEnableBasedOnChannel(prefs(),
                                         version_info::Channel::UNKNOWN);
  EXPECT_NO_FATAL_FAILURE(ExpectBreadcrumbsStatusIs(false, disabled_time));
}

// Tests that if breadcrumbs is enabled in user prefs but the prefs' timestamp
// has expired, the prefs will be overwritten and the timestamp updated.
TEST_F(BreadcrumbsStatusTest, OverwriteExpiredPrefs) {
  const auto expired_time = base::Time::Now() - base::Days(31);
  SetBreadcrumbsEnabledPrefs(/*is_enabled=*/true, expired_time);
  breadcrumbs::MaybeEnableBasedOnChannel(prefs(),
                                         version_info::Channel::UNKNOWN);
  EXPECT_NO_FATAL_FAILURE(ExpectBreadcrumbsStatusIs(false, base::Time::Now()));
}

// Tests that if breadcrumbs is enabled in user prefs but the prefs' timestamp
// is in the future, the prefs will be overwritten and the timestamp updated.
TEST_F(BreadcrumbsStatusTest, OverwriteInvalidPrefs) {
  const auto invalid_time = base::Time::Now() + base::Days(5);
  SetBreadcrumbsEnabledPrefs(/*is_enabled=*/true, invalid_time);
  breadcrumbs::MaybeEnableBasedOnChannel(prefs(),
                                         version_info::Channel::UNKNOWN);
  EXPECT_NO_FATAL_FAILURE(ExpectBreadcrumbsStatusIs(false, base::Time::Now()));
}

// Tests that if breadcrumbs' user prefs have not been set, the prefs will be
// written and the timestamp set to the current time.
TEST_F(BreadcrumbsStatusTest, OverwriteMissingPrefs) {
  ASSERT_FALSE(prefs()->HasPrefPath(breadcrumbs::kEnabledPref));
  ASSERT_FALSE(prefs()->HasPrefPath(breadcrumbs::kEnabledTimePref));
  breadcrumbs::MaybeEnableBasedOnChannel(prefs(),
                                         version_info::Channel::UNKNOWN);
  EXPECT_NO_FATAL_FAILURE(ExpectBreadcrumbsStatusIs(false, base::Time::Now()));
}

}  // namespace breadcrumbs
