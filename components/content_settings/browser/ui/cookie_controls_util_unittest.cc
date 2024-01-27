// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_util.h"

#include "base/environment.h"
#include "base/test/icu_test_util.h"
#include "base/time/time_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {
namespace {
using ::testing::Eq;

const char kNewYorkTime[] = "America/New_York";

struct StaticOverrideTime {
  static base::Time Now() { return override_time; }
  static base::Time override_time;
};

base::Time StaticOverrideTime::override_time;

// A scoped wrapper that uses both tzset() and ScopedRestoreDefaultTimezone.
//
// Some platforms use posix time and some use ICU time.
// ScopedRestoreDefaultTimezone only supports systems using ICU time, so without
// setting the timezone both ways timezone dependent tests fail on some
// platforms.
//
// Not thread safe.
class ScopedMockTimezone {
 public:
  explicit ScopedMockTimezone(const std::string& timezone)
      : icu_timezone_(timezone.c_str()) {
    auto env = base::Environment::Create();
    std::string old_timezone_value;
    if (env->GetVar(kTZ, &old_timezone_value)) {
      old_timezone_ = old_timezone_value;
    }
    CHECK(env->SetVar(kTZ, timezone));
    tzset();
  }

  ~ScopedMockTimezone() {
    auto env = base::Environment::Create();
    if (old_timezone_.has_value()) {
      CHECK(env->SetVar(kTZ, old_timezone_.value()));
    } else {
      CHECK(env->UnSetVar(kTZ));
    }
  }

  ScopedMockTimezone(const ScopedMockTimezone& other) = delete;
  ScopedMockTimezone& operator=(const ScopedMockTimezone& other) = delete;

 private:
  static constexpr char kTZ[] = "TZ";

  base::test::ScopedRestoreDefaultTimezone icu_timezone_;
  std::optional<std::string> old_timezone_;
};
}  // namespace

class CookieControlsUtilTest : public ::testing::Test {
 public:
  base::Time GetTime(const char* time_str) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString(time_str, &time));
    return time;
  }

  base::subtle::ScopedTimeClockOverrides GetScopedNow(const char* time_str) {
    StaticOverrideTime::override_time = GetTime(time_str);
    return base::subtle::ScopedTimeClockOverrides(
        &StaticOverrideTime::Now,
        /*time_ticks_override=*/nullptr,
        /*thread_ticks_override=*/nullptr);
  }
};

// Return value of 0 represents times that occur today.
TEST_F(CookieControlsUtilTest, Today) {
  auto time_override = GetScopedNow("Tue, 15 Nov 2023 12:45:26");
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 15 Nov 2023 12:51:26")),
              Eq(0));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 15 Nov 2023 12:45:26")),
              Eq(0));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 15 Nov 2023 23:59:59")),
              Eq(0));
}

// Return value of 1 represents times that occur tomorrow.
TEST_F(CookieControlsUtilTest, Tomorrow) {
  auto time_override = GetScopedNow("Tue, 15 Nov 2023 12:45:26");
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 16 Nov 2023 00:00:01")),
              Eq(1));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 16 Nov 2023 5:45:25")),
              Eq(1));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 16 Nov 2023 12:45:25")),
              Eq(1));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 16 Nov 2023 12:45:26")),
              Eq(1));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 16 Nov 2023 23:59:59")),
              Eq(1));
}

TEST_F(CookieControlsUtilTest, TwoDays) {
  auto time_override = GetScopedNow("Tue, 15 Nov 2023 12:45:26");
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 17 Nov 2023 00:00:01")),
              Eq(2));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 17 Nov 2023 5:45:25")),
              Eq(2));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 17 Nov 2023 12:45:25")),
              Eq(2));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 17 Nov 2023 12:45:26")),
              Eq(2));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 17 Nov 2023 23:59:59")),
              Eq(2));
}

TEST_F(CookieControlsUtilTest, Past) {
  auto time_override = GetScopedNow("Tue, 15 Nov 2023 12:45:26");
  // Still today
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 15 Nov 2023 12:51:25")),
              Eq(0));
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 15 Nov 2023 00:00:00")),
              Eq(0));
  // Yesterday
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 14 Nov 2023 23:59:59")),
              Eq(-1));
  // Two days ago
  EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(
                  GetTime("Tue, 13 Nov 2023 12:45:26")),
              Eq(-2));
}

// On Fuchsia posix local time functions always use UTC.
#if !BUILDFLAG(IS_FUCHSIA)
// For 2023 DST for New York timezone is from March 12 to November 5.
TEST_F(CookieControlsUtilTest, DSTOverlap) {
  ScopedMockTimezone scoped_timezone(kNewYorkTime);
  {
    // 23:00, spring forward to next day, so we actually expire on the 6th day
    // for 5 day period.
    auto time_override = GetScopedNow("Sat, 11 Mar 2023 23:00:00");
    EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                        base::Days(5)),
                Eq(6));
  }
  {
    // 22:00, spring forward, but not quite to the next day.
    auto time_override = GetScopedNow("Sat, 11 Mar 2023 22:00:00");
    EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                        base::Days(5)),
                Eq(5));
  }
  {
    // 00:00, fall back to the previous day (so day 4 for a 5 day period).
    auto time_override = GetScopedNow("Sat, 4 Nov 2023 00:00:00");
    EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                        base::Days(5)),
                Eq(4));
  }
  {
    // 01:00, fall back but not quite to the previous day.
    auto time_override = GetScopedNow("Sat, 4 Nov 2023 01:00:00");
    EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                        base::Days(5)),
                Eq(5));
  }
}
#endif

TEST_F(CookieControlsUtilTest, NoDSTOverlapOutsideDST) {
  base::test::ScopedRestoreDefaultTimezone scoped_timezone(kNewYorkTime);
  // Without DST overlap expiration should be in days equal to the actual number
  // of 24hr periods we expire from now.
  {
    auto time_override = GetScopedNow("Sat, 1 Jan 2023 00:00:00");
    for (int i = 0; i < 60; i++) {
      EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                          base::Days(i)),
                  Eq(i));
    }
  }
  {
    auto time_override = GetScopedNow("Sat, 1 Jan 2023 23:00:00");
    for (int i = 0; i < 60; i++) {
      EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                          base::Days(i)),
                  Eq(i));
    }
  }
}

TEST_F(CookieControlsUtilTest, NoDSTOverlapInDST) {
  base::test::ScopedRestoreDefaultTimezone scoped_timezone(kNewYorkTime);
  // Without DST overlap expiration should be in days equal to the actual number
  // of 24hr periods we expire from now.
  {
    auto time_override = GetScopedNow("Sat, 1 Apr 2023 00:00:00");
    for (int i = 0; i < 60; i++) {
      EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                          base::Days(i)),
                  Eq(i));
    }
  }
  {
    auto time_override = GetScopedNow("Sat, 1 Apr 2023 23:00:00");
    for (int i = 0; i < 60; i++) {
      EXPECT_THAT(CookieControlsUtil::GetDaysToExpiration(base::Time::Now() +
                                                          base::Days(i)),
                  Eq(i));
    }
  }
}

}  // namespace content_settings
