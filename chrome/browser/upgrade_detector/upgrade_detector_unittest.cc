// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/upgrade_detector.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/test/scoped_libc_timezone_override.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestUpgradeDetector : public UpgradeDetector {
 public:
  explicit TestUpgradeDetector(const base::Clock* clock,
                               const base::TickClock* tick_clock)
      : UpgradeDetector(clock, tick_clock) {}
  TestUpgradeDetector(const TestUpgradeDetector&) = delete;
  TestUpgradeDetector& operator=(const TestUpgradeDetector&) = delete;
  ~TestUpgradeDetector() override = default;

  // Overriding pure virtual functions for testing.
  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override {
    return base::Time();
  }

  // Exposed for testing.
  using UpgradeDetector::AdjustDeadline;
  using UpgradeDetector::GetRelaunchWindowPolicyValue;
};

}  // namespace

class UpgradeDetectorTest : public ::testing::Test {
 protected:
  UpgradeDetectorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  const base::Clock* GetMockClock() { return task_environment_.GetMockClock(); }

  const base::TickClock* GetMockTickClock() {
    return task_environment_.GetMockTickClock();
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void OverrideTimezone(const std::string& tz) {
    // If there already is the override, reset it first.
    libc_timezone_override_.reset();
    libc_timezone_override_.emplace(tz);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Sets the browser.relaunch_window preference in Local State.
  void SetRelaunchWindowPref(int hour, int minute, int duration_mins) {
    // Create the dict representing relaunch time interval.
    base::Value::Dict entry;
    entry.SetByDottedPath("start.hour", hour);
    entry.SetByDottedPath("start.minute", minute);
    entry.Set("duration_mins", duration_mins);
    // Put it in a list.
    base::Value::List entries;
    entries.Append(std::move(entry));
    // Put the list in the policy value.
    base::Value::Dict value;
    value.Set("entries", std::move(entries));

    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetManagedPref(
        prefs::kRelaunchWindow, base::Value(std::move(value)));
  }

  UpgradeDetector::RelaunchWindow CreateRelaunchWindow(int hour,
                                                       int minute,
                                                       int duration_mins) {
    return UpgradeDetector::RelaunchWindow(hour, minute,
                                           base::Minutes(duration_mins));
  }

 private:
  base::test::TaskEnvironment task_environment_;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::optional<base::test::ScopedLibcTimezoneOverride> libc_timezone_override_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
};

TEST_F(UpgradeDetectorTest, RelaunchWindowPolicy) {
  TestUpgradeDetector upgrade_detector(GetMockClock(), GetMockTickClock());
  // Relaunch window pref is not set.
  EXPECT_FALSE(upgrade_detector.GetRelaunchWindowPolicyValue());

  // Set relaunch window from 2:20am to 5:20am.
  SetRelaunchWindowPref(/*hour=*/2, /*minute=*/20, /*duration_mins=*/180);
  std::optional<UpgradeDetector::RelaunchWindow> window =
      upgrade_detector.GetRelaunchWindowPolicyValue();
  ASSERT_TRUE(window);
  EXPECT_EQ(window.value().hour, 2);
  EXPECT_EQ(window.value().minute, 20);
  EXPECT_EQ(window.value().duration, base::Minutes(180));
}

TEST_F(UpgradeDetectorTest, DeadlineAdjustment) {
  TestUpgradeDetector upgrade_detector(GetMockClock(), GetMockTickClock());
  // Get relaunch window from 2:20am to 5:20am.
  const auto window =
      CreateRelaunchWindow(/*hour=*/2, /*minute=*/20, /*duration_mins=*/180);

  // Deadline is adjusted to fall within relaunch window on next day.
  base::Time high_deadline;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 06:00", &high_deadline));
  base::Time adjusted_deadline, deadline_lower_border, deadline_upper_border;
  ASSERT_TRUE(
      base::Time::FromString("2 Jan 2018 02:20", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("2 Jan 2018 05:20", &deadline_upper_border));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_GE(adjusted_deadline, deadline_lower_border);
  EXPECT_LT(adjusted_deadline, deadline_upper_border);

  // Deadline is adjusted to fall within relaunch window on the same day.
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 01:00", &high_deadline));
  ASSERT_TRUE(
      base::Time::FromString("1 Jan 2018 02:20", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("1 Jan 2018 05:20", &deadline_upper_border));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_GE(adjusted_deadline, deadline_lower_border);
  EXPECT_LT(adjusted_deadline, deadline_upper_border);

  // No change in the deadline as it already within relaunch window.
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 03:00", &high_deadline));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_EQ(adjusted_deadline, high_deadline);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorTest, DeadlineAdjustmentFor24HrsDuration) {
  TestUpgradeDetector upgrade_detector(GetMockClock(), GetMockTickClock());
  const auto window =
      CreateRelaunchWindow(/*hour=*/20, /*minute*/ 30, /*duration_mins=*/1440);

  // No change in the deadline as relaunch window covers whole day.
  base::Time high_deadline;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 06:00", &high_deadline));
  base::Time adjusted_deadline =
      upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_EQ(adjusted_deadline, high_deadline);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorTest, DeadlineAdjustmentForOneDuration) {
  TestUpgradeDetector upgrade_detector(GetMockClock(), GetMockTickClock());
  // Get relaunch window for single time point 8:30pm.
  const auto window =
      CreateRelaunchWindow(/*hour=*/20, /*minute=*/30, /*duration_mins=*/1);

  base::Time high_deadline;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 06:00", &high_deadline));
  base::Time adjusted_deadline, deadline_lower_border, deadline_upper_border;
  ASSERT_TRUE(
      base::Time::FromString("1 Jan 2018 20:30", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("1 Jan 2018 20:31", &deadline_upper_border));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_GE(adjusted_deadline, deadline_lower_border);
  EXPECT_LT(adjusted_deadline, deadline_upper_border);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

TEST_F(UpgradeDetectorTest, DeadlineAdjustmentOverMidnight) {
  TestUpgradeDetector upgrade_detector(GetMockClock(), GetMockTickClock());
  // Get relaunch window from 11:10pm to 2:10am.
  const auto window =
      CreateRelaunchWindow(/*hour=*/23, /*minute=*/10, /*duration_mins=*/180);

  // Deadline is adjusted to fall within relaunch window on the same day.
  base::Time high_deadline;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 16:00", &high_deadline));
  base::Time adjusted_deadline, deadline_lower_border, deadline_upper_border;
  ASSERT_TRUE(
      base::Time::FromString("1 Jan 2018 23:10", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("2 Jan 2018 02:10", &deadline_upper_border));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_GE(adjusted_deadline, deadline_lower_border);
  EXPECT_LT(adjusted_deadline, deadline_upper_border);

  // No change in the deadline post midnight and within the relaunch window.
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 00:20", &high_deadline));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_EQ(adjusted_deadline, high_deadline);

  // No change in the deadline pre midnight and within the relaunch window.
  ASSERT_TRUE(base::Time::FromString("1 Jan 2018 23:35", &high_deadline));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_EQ(adjusted_deadline, high_deadline);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(UpgradeDetectorTest, DeadlineAdjustmentDst) {
  // Set Europe timezone where daylight saving starts (UTC+1) at local 2:00am
  // on the last Sunday of March and ends at local 3:00am on the last Sunday of
  // October.
  OverrideTimezone("CET-1CEST,M3.5.0/2,M10.5.0/3");
  TestUpgradeDetector upgrade_detector(GetMockClock(), GetMockTickClock());
  // Get relaunch window from 12:10am to 12:40am.
  const auto window =
      CreateRelaunchWindow(/*hour=*/0, /*minute=*/10, /*duration_mins=*/30);

  // Clocks are set forward on 28 March 2021 2:00am local time.
  base::Time high_deadline;
  ASSERT_TRUE(base::Time::FromString("27 Mar 2021 23:30", &high_deadline));
  base::Time adjusted_deadline, deadline_lower_border, deadline_upper_border;
  ASSERT_TRUE(
      base::Time::FromString("28 Mar 2021 0:10", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("28 Mar 2021 0:40", &deadline_upper_border));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_GE(adjusted_deadline, deadline_lower_border);
  EXPECT_LT(adjusted_deadline, deadline_upper_border);

  // Set North America EST timezone where daylight saving starts (UTC-4) at
  // local 2:00am on the second Sunday of March and ends at local 2:00am on the
  // first Sunday of November.
  OverrideTimezone("EST+5EDT,M3.2.0/2,M11.1.0/2");

  // Clocks are set forward on 14 March 2021 2:00am local time.
  ASSERT_TRUE(base::Time::FromString("13 Mar 2021 23:30", &high_deadline));
  ASSERT_TRUE(
      base::Time::FromString("14 Mar 2021 0:10", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("14 Mar 2021 0:40", &deadline_upper_border));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_GE(adjusted_deadline, deadline_lower_border);
  EXPECT_LT(adjusted_deadline, deadline_upper_border);

  // Set Cuba timezone where daylight saving starts (UTC-4) at local midnight
  // on the second Sunday of March and ends at local midnight on the first
  // Sunday of November.
  OverrideTimezone("EST+5EDT,M3.2.0/0,M11.1.0/0");

  // Clocks are set back on 7 Nov 2021 12:00am local time.
  ASSERT_TRUE(base::Time::FromString("6 Nov 2021 00:50", &high_deadline));
  ASSERT_TRUE(
      base::Time::FromString("7 Nov 2021 0:10", &deadline_lower_border));
  ASSERT_TRUE(
      base::Time::FromString("7 Nov 2021 0:40", &deadline_upper_border));
  adjusted_deadline = upgrade_detector.AdjustDeadline(high_deadline, window);
  EXPECT_GE(adjusted_deadline, deadline_lower_border);
  EXPECT_LT(adjusted_deadline, deadline_upper_border);

  upgrade_detector.Shutdown();
  RunUntilIdle();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
