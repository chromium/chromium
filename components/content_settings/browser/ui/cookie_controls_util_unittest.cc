// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_util.h"

#include "base/time/time_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {
namespace {
using ::testing::Eq;

struct StaticOverrideTime {
  static base::Time Now() { return override_time; }
  static base::Time override_time;
};

base::Time StaticOverrideTime::override_time;
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
  // EXPECT_THAT(task_environment_.GetMockClock()->Now(), Eq(GetTime("Tue, 15
  // Nov 2023 12:45:26")));
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

}  // namespace content_settings
