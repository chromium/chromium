// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/date_changed_helper.h"

#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using base::subtle::TimeNowIgnoringOverride;

const char kTestPrefName[] = "TestPref";

// TODO(crbug.com/40099277): Use TaskEnvironment::TimeSource::MOCK_TIME here
// instead of explicit clock overrides when it better supports setting a
// specific time of day and rewinding time.
class DateChangedHelperTest : public testing::Test {
 public:
  void SetUp() override {
    date_changed_helper::RegisterPref(prefs_.registry(), kTestPrefName);
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

}  // namespace

// Should not consider date changed if the preference is not available.
TEST_F(DateChangedHelperTest, TestNewDoesNotFire) {
  SCOPED_TRACE(base::Time::Now());
  ASSERT_FALSE(
      date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName));
}

// Should consider date changed if the preference is more than a day old.
TEST_F(DateChangedHelperTest, TestOldFires) {
  SCOPED_TRACE(base::Time::Now());
  date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName);

  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return TimeNowIgnoringOverride() + base::Hours(25); }, nullptr,
      nullptr);
  ASSERT_TRUE(
      date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName));
}

// Should consider date changed if the preference is more than a day in the
// future.
TEST_F(DateChangedHelperTest, TestFutureFires) {
  SCOPED_TRACE(base::Time::Now());
  date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName);

  base::subtle::ScopedTimeClockOverrides time_override(
      []() { return TimeNowIgnoringOverride() - base::Hours(25); }, nullptr,
      nullptr);
  ASSERT_TRUE(
      date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName));
}

// Should not consider date changed if the preference is earlier the same day.
TEST_F(DateChangedHelperTest, TestEarlierSameDayNotFired) {
  SCOPED_TRACE(base::Time::Now());
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          return TimeNowIgnoringOverride().LocalMidnight() + base::Hours(2);
        },
        nullptr, nullptr);
    date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName);
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        return TimeNowIgnoringOverride().LocalMidnight() + base::Hours(22);
      },
      nullptr, nullptr);
  ASSERT_FALSE(
      date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName));
}

// Should not consider date changed if the preference is later the same day.
TEST_F(DateChangedHelperTest, TestLaterSameDayNotFired) {
  SCOPED_TRACE(base::Time::Now());
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          return TimeNowIgnoringOverride().LocalMidnight() + base::Hours(22);
        },
        nullptr, nullptr);
    date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName);
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        return TimeNowIgnoringOverride().LocalMidnight() + base::Hours(2);
      },
      nullptr, nullptr);
  ASSERT_FALSE(
      date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName));
}

// Should consider date changed if the preference is in the previous day.
TEST_F(DateChangedHelperTest, TestJustNextDayFired) {
  SCOPED_TRACE(base::Time::Now());
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          return TimeNowIgnoringOverride().LocalMidnight() - base::Minutes(5);
        },
        nullptr, nullptr);
    date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName);
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        return TimeNowIgnoringOverride().LocalMidnight() + base::Minutes(5);
      },
      nullptr, nullptr);
  ASSERT_TRUE(
      date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName));
}

// Should consider date changed if the preference is in the next day.
TEST_F(DateChangedHelperTest, TestJustPreviousDayFired) {
  SCOPED_TRACE(base::Time::Now());
  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          return TimeNowIgnoringOverride().LocalMidnight() + base::Minutes(5);
        },
        nullptr, nullptr);
    date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName);
  }

  base::subtle::ScopedTimeClockOverrides time_override(
      []() {
        return TimeNowIgnoringOverride().LocalMidnight() - base::Minutes(5);
      },
      nullptr, nullptr);
  ASSERT_TRUE(
      date_changed_helper::HasDateChangedSinceLastCall(&prefs_, kTestPrefName));
}

}  // namespace metrics
