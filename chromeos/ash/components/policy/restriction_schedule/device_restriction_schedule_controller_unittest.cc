// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/policy/weekly_time/test_support.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval_checked.h"
#include "chromeos/constants/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Empty list.
constexpr const char* kPolicyJsonEmpty = "[]";

// List members have to be dicts so this is invalid.
constexpr const char* kPolicyJsonInvalid = "[1]";

// Sample policy value. Times are 12:00, 21:00, 18:00, 06:00.
constexpr const char* kPolicyJson = R"([
  {
    "start": {
        "day_of_week": "WEDNESDAY",
        "milliseconds_since_midnight": 43200000
    },
    "end": {
        "day_of_week": "WEDNESDAY",
        "milliseconds_since_midnight": 75600000
    }
  },
  {
    "start": {
        "day_of_week": "FRIDAY",
        "milliseconds_since_midnight": 64800000
    },
    "end": {
        "day_of_week": "MONDAY",
        "milliseconds_since_midnight": 21600000
    }
  }
])";

constexpr const char kTZ[] = "TZ";

using weekly_time::BuildList;
using weekly_time::DayToString;
using weekly_time::TimeFromString;
using Day = WeeklyTimeChecked::Day;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;
using testing::Return;

// Used to verify time in EXPECT_CALLs. Macro in order to see correct line
// numbers in errors.
#define EXPECT_TIME(day, hours, minutes)                                     \
  [] {                                                                       \
    auto actual = WeeklyTimeChecked::FromTimeAsLocalTime(base::Time::Now()); \
    int actual_minutes = actual.milliseconds_since_midnight() / 1000 / 60;   \
    int actual_hours = actual_minutes / 60;                                  \
    actual_minutes -= actual_hours * 60;                                     \
    EXPECT_EQ(actual.day_of_week(), day);                                    \
    EXPECT_EQ(actual_hours, hours);                                          \
    EXPECT_EQ(actual_minutes, minutes);                                      \
  }

// Used to verify time in EXPECT_CALLs. Macro in order to see correct line
// numbers in errors.
#define EXPECT_TIME_STR(time_str)                          \
  [&] {                                                    \
    base::Time expected_time = TimeFromString((time_str)); \
    base::Time actual_time = base::Time::Now();            \
    EXPECT_EQ(actual_time, expected_time);                 \
  }

}  // namespace

class MockDelegate : public DeviceRestrictionScheduleController::Delegate {
 public:
  // DeviceRestrictionScheduleController::Delegate:
  MOCK_CONST_METHOD0(IsUserLoggedIn, bool());
  MOCK_METHOD1(ShowUpcomingLogoutNotification, void(base::Time));
  MOCK_METHOD0(ShowPostLogoutNotification, void());
};

class MockObserver : public DeviceRestrictionScheduleController::Observer {
 public:
  // DeviceRestrictionScheduleController::Observer:
  MOCK_METHOD1(OnRestrictionScheduleStateChanged, void(bool));
  MOCK_METHOD0(OnRestrictionScheduleMessageChanged, void());
};

class DeviceRestrictionScheduleControllerTest : public testing::Test {
 public:
  DeviceRestrictionScheduleControllerTest() {
    DeviceRestrictionScheduleController::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void SetUp() override {
    ash::LoginState::Initialize();
    controller_ = std::make_unique<DeviceRestrictionScheduleController>(
        delegate_, local_state_);
    controller_->AddObserver(&observer_);
  }

  void TearDown() override {
    controller_->RemoveObserver(&observer_);
    controller_.reset();
    ash::LoginState::Shutdown();
  }

  void UpdatePolicyPref(const char* policy_json) {
    local_state_.SetList(chromeos::prefs::kDeviceRestrictionSchedule,
                         BuildList(policy_json));
  }

  void AdvanceTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetTime(const char* time_str) {
    base::Time time = TimeFromString(time_str);
    base::TimeDelta delta = time - base::Time::Now();
    CHECK(!delta.is_negative());
    AdvanceTime(delta);
  }

  void SetTime(Day day, int hours, int minutes) {
    const int millis =
        (base::Hours(hours) + base::Minutes(minutes)).InMilliseconds();
    auto time = WeeklyTimeChecked(day, millis);
    auto current_time =
        WeeklyTimeChecked::FromTimeAsLocalTime(base::Time::Now());
    base::TimeDelta delta =
        WeeklyTimeIntervalChecked(current_time, time).Duration();
    AdvanceTime(delta);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  NiceMock<MockDelegate> delegate_;
  NiceMock<MockObserver> observer_;
  std::unique_ptr<DeviceRestrictionScheduleController> controller_;
};

// Should do nothing (intervals didn't change).
TEST_F(DeviceRestrictionScheduleControllerTest, EmptyPolicy) {
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(_)).Times(0);
  UpdatePolicyPref(kPolicyJsonEmpty);
}

// Should do nothing (intervals didn't change).
TEST_F(DeviceRestrictionScheduleControllerTest, InvalidPolicy) {
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(_)).Times(0);
  UpdatePolicyPref(kPolicyJsonInvalid);
}

// Going from non-empty regular to empty should reset everything.
TEST_F(DeviceRestrictionScheduleControllerTest, NonEmptyRegularToEmptyPolicy) {
  // Set time outside restricted schedule.
  SetTime(Day::kTuesday, 0, 0);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false)).Times(1);
  UpdatePolicyPref(kPolicyJsonEmpty);

  // Advance for a full week. Nothing should be called anymore since the policy
  // isn't active.
  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(0);
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_)).Times(0);
  EXPECT_CALL(delegate_, ShowPostLogoutNotification()).Times(0);
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(_)).Times(0);

  AdvanceTime(base::Days(7));
}

// Going from non-empty restricted to invalid should reset everything.
TEST_F(DeviceRestrictionScheduleControllerTest,
       NonEmptyRestrictedToInvalidPolicy) {
  // Set time inside restricted schedule.
  SetTime(Day::kSaturday, 0, 0);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false)).Times(1);
  UpdatePolicyPref(kPolicyJsonInvalid);

  // Advance for a full week. Nothing should be called anymore since the policy
  // isn't active.
  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(0);
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_)).Times(0);
  EXPECT_CALL(delegate_, ShowPostLogoutNotification()).Times(0);
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(_)).Times(0);

  AdvanceTime(base::Days(7));
}

// Verify the whole flow of the sample policy starting from regular state.
TEST_F(DeviceRestrictionScheduleControllerTest, SamplePolicyRegularStart) {
  // Set time outside restricted schedule.
  SetTime(Day::kWednesday, 0, 0);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  // Upcoming logout notification should be shown at Wed 11:30.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kWednesday, 11, 30), Return(true)));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kWednesday, 11, 30));

  // Next restricted period should start at Wed 12:00.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kWednesday, 12, 0), Return(true)));
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kWednesday, 12, 0));

  // Next regular period should start at Wed 21:00.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kWednesday, 21, 0));

  // Upcoming logout notification would normally be shown at Fri 17:30, but it
  // is not shown since a user session wasn't in progress.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kFriday, 17, 30), Return(false)));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_)).Times(0);

  // Next restricted period should start at Fri 18:00.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kFriday, 18, 0), Return(true)));
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kFriday, 18, 0));

  // Next regular period should start at Mon 06:00.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kMonday, 6, 0));

  // Advance for a full week. Will verify the whole schedule with EXPECT_CALLs.
  AdvanceTime(base::Days(7));
}

// Verify the whole flow of the sample policy starting from restricted state.
TEST_F(DeviceRestrictionScheduleControllerTest, SamplePolicyRestrictedStart) {
  // Set time inside restricted schedule.
  SetTime(Day::kSaturday, 0, 0);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1);
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  // Next regular period should start at Mon 06:00.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kMonday, 6, 0));

  // Upcoming logout notification should be shown at Wed 11:30.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kWednesday, 11, 30), Return(true)));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kWednesday, 11, 30));

  // Next restricted period should start at Wed 12:00.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kWednesday, 12, 0), Return(true)));
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kWednesday, 12, 0));

  // Next regular period should start at Wed 21:00.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kWednesday, 21, 0));

  // Upcoming logout notification should be shown at Fri 17:30.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kFriday, 17, 30), Return(true)));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kFriday, 17, 30));

  // Next restricted period should start at Fri 18:00.
  EXPECT_CALL(delegate_, IsUserLoggedIn())
      .Times(1)
      .WillOnce(DoAll(EXPECT_TIME(Day::kFriday, 18, 0), Return(true)));
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true))
      .Times(1)
      .WillOnce(EXPECT_TIME(Day::kFriday, 18, 0));

  // Advance for a full week. Will verify the whole schedule with EXPECT_CALLs.
  AdvanceTime(base::Days(7));
}

// Verify that entering restricted schedule inside a user session enables the
// `chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification` pref.
TEST_F(DeviceRestrictionScheduleControllerTest,
       ShowPostLogoutNotification_PrefIsSet) {
  // Set time inside restricted schedule.
  SetTime(Day::kSunday, 0, 0);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  EXPECT_TRUE(local_state_.GetBoolean(
      chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification));
}

// Verify that entering restricted schedule outside a user session does not set
// the `chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification`
// pref.
TEST_F(DeviceRestrictionScheduleControllerTest,
       ShowPostLogoutNotification_PrefIsNotSet) {
  // Set time inside restricted schedule.
  SetTime(Day::kSunday, 0, 0);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  EXPECT_FALSE(local_state_.HasPrefPath(
      chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification));
}

// Verify that `ShowUpcomingLogoutNotification` is called immediately if there's
// less than 30 minutes until restricted schedule begins and a user session is
// active.
TEST_F(DeviceRestrictionScheduleControllerTest,
       ShowUpcomingLogoutNotification_CalledImmediately) {
  // Set time 20 minutes before restricted schedule.
  SetTime(Day::kWednesday, 11, 40);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false)).Times(1);
  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  // Run any pending timers.
  AdvanceTime(base::TimeDelta());
}

// Verify that `ShowUpcomingLogoutNotification` isn't called if a user session
// isn't in progress.
TEST_F(DeviceRestrictionScheduleControllerTest,
       ShowUpcomingLogoutNotification_NotCalled) {
  // Set time 20 minutes before restricted schedule.
  SetTime(Day::kWednesday, 11, 40);
  // Make sure all EXPECT_CALLs are in sequence.
  InSequence seq;

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false)).Times(1);
  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_)).Times(0);
  UpdatePolicyPref(kPolicyJson);

  // Run any pending timers.
  AdvanceTime(base::TimeDelta());
}

// Verify that `ShowUpcomingLogoutNotification` is called after login if there's
// less than 30 minutes until restricted schedule begins.
TEST_F(DeviceRestrictionScheduleControllerTest,
       ShowUpcomingLogoutNotification_CalledAfterLogin) {
  // Set time 20 minutes before restricted schedule.
  SetTime(Day::kWednesday, 11, 40);

  // Not logged in, notification doesn't show.
  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_)).Times(0);
  UpdatePolicyPref(kPolicyJson);

  // Run any pending timers.
  AdvanceTime(base::TimeDelta());
  Mock::VerifyAndClearExpectations(&observer_);

  // Logged in, notification shows.
  EXPECT_CALL(delegate_, IsUserLoggedIn()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(delegate_, ShowUpcomingLogoutNotification(_)).Times(1);

  // Perform login.
  ash::LoginState::Get()->SetLoggedInState(
      ash::LoginState::LOGGED_IN_ACTIVE,
      ash::LoginState::LOGGED_IN_USER_REGULAR);

  // Run any pending timers.
  AdvanceTime(base::TimeDelta());
  Mock::VerifyAndClearExpectations(&observer_);
}

class DeviceRestrictionScheduleControllerTestShowPostLogoutNotification
    : public DeviceRestrictionScheduleControllerTest {
 public:
  // Manually driven inside the tests to allow custom pre-setup.
  void SetUp() override {}
};

// Verify that `ShowPostLogoutNotification` is called during startup if the
// `chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification` pref
// was set to true.
TEST_F(DeviceRestrictionScheduleControllerTestShowPostLogoutNotification,
       PrefTrue_Shown) {
  local_state_.SetBoolean(
      chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification,
      true);

  // Notification is shown.
  EXPECT_CALL(delegate_, ShowPostLogoutNotification()).Times(1);

  // This call creates the controller which then does some startup time logic.
  DeviceRestrictionScheduleControllerTest::SetUp();

  // Pref was reset.
  EXPECT_FALSE(local_state_.GetBoolean(
      chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification));
}

// Verify that `ShowPostLogoutNotification` is not called during startup if the
// `chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification` pref
// was set to false.
TEST_F(DeviceRestrictionScheduleControllerTestShowPostLogoutNotification,
       PrefFalse_NotShown) {
  local_state_.SetBoolean(
      chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification,
      false);

  // Notification is not shown.
  EXPECT_CALL(delegate_, ShowPostLogoutNotification()).Times(0);

  // This call creates the controller which then does some startup time logic.
  DeviceRestrictionScheduleControllerTest::SetUp();
}

// Verify that `ShowPostLogoutNotification` is not called during startup if the
// `chromeos::prefs::kDeviceRestrictionScheduleShowPostLogoutNotification` pref
// was not set.
TEST_F(DeviceRestrictionScheduleControllerTestShowPostLogoutNotification,
       PrefUnset_NotShown) {
  // Notification is not shown.
  EXPECT_CALL(delegate_, ShowPostLogoutNotification()).Times(0);

  // This call creates the controller which then does some startup time logic.
  DeviceRestrictionScheduleControllerTest::SetUp();
}

// Verify `RestrictionScheduleEndDay` & `RestrictionScheduleEndTime` functions.
TEST_F(DeviceRestrictionScheduleControllerTest, RestrictionScheduleEndDayTime) {
  // clang-format off
  const struct TestData {
    WeeklyTimeChecked::Day day;
    int hours;
    int minutes;
    std::u16string expected_day;
    std::u16string expected_time;
  } kTestData[] = {
    // Inside restriction schedule, verify end time.
    {Day::kWednesday, 15, 0, u"today",       u"9:00\u202fPM"},
    {Day::kFriday,    19, 0, u"on Monday",   u"6:00\u202fAM"},
    {Day::kSaturday,  19, 0, u"on Monday",   u"6:00\u202fAM"},
    {Day::kSunday,    19, 0, u"tomorrow",    u"6:00\u202fAM"},
    {Day::kMonday,     1, 0, u"today",       u"6:00\u202fAM"},
    // Inside regular schedule, verify that empty strings are returned.
    {Day::kWednesday, 10, 0, u"", u""},
    {Day::kTuesday,   10, 0, u"", u""},
    {Day::kMonday,    10, 0, u"", u""},
    {Day::kWednesday, 22, 0, u"", u""},
  };
  // clang-format on

  for (const auto& t : kTestData) {
    SetTime(t.day, t.hours, t.minutes);
    UpdatePolicyPref(kPolicyJson);
    SCOPED_TRACE(testing::Message()
                 << "day: " << DayToString(t.day) << ", hours: " << t.hours
                 << ", minutes: " << t.minutes);
    EXPECT_EQ(t.expected_day, controller_->RestrictionScheduleEndDay());
    EXPECT_EQ(t.expected_time, controller_->RestrictionScheduleEndTime());
  }
}

// Verify that the restriction schedule banner message is updated appropriately.
// Also tests edge cases around handling of DST changes.
TEST_F(DeviceRestrictionScheduleControllerTest,
       RestrictionScheduleMessageChanged) {
  // clang-format off
  constexpr const struct TestData {
    const char* start_time;
    const char* sunday_midnight_utc;
    const char* monday_midnight_utc;
  } kTestData[] = {
      // Regular Friday.
      {"Fri 22 Mar 2024 19:00",
       "Sat 23 Mar 2024 23:00 GMT",
       "Sun 24 Mar 2024 23:00 GMT"},
      // DST starts on Sun, 31 Mar 2024 when the clock moves from 2:00 to 3:00,
      // this is 2 days before on a Friday.
      {"Fri 29 Mar 2024 19:00",
       "Sat 30 Mar 2024 23:00 GMT",
       "Sun 31 Mar 2024 22:00 GMT"},
      // DST ends on Sun, 27 Oct 2024 when the clock moves from 3:00 to 2:00,
      // this is 2 days before on a Friday.
      {"Fri 25 Oct 2024 19:00",
       "Sat 26 Oct 2024 22:00 GMT",
       "Sun 27 Oct 2024 23:00 GMT"},
  };
  // clang-format on

  // Override the local time zone for the current test to have it fixed.
  base::ScopedEnvironmentVariableOverride scoped_timezone(kTZ, "Europe/Berlin");

  for (const auto& t : kTestData) {
    // Start each test case with a clean state.
    UpdatePolicyPref(kPolicyJsonEmpty);

    // Set time inside restricted schedule.
    SetTime(t.start_time);  // Friday 19:00
    SCOPED_TRACE(testing::Message() << "time: " << t.start_time);

    // The text should initially contain "Monday" and the changed function
    // shouldn't be called.
    EXPECT_CALL(observer_, OnRestrictionScheduleMessageChanged()).Times(0);
    UpdatePolicyPref(kPolicyJson);
    EXPECT_EQ(u"on Monday", controller_->RestrictionScheduleEndDay());

    // Nothing happens yet.
    AdvanceTime(base::Days(1));  // Saturday 19:00
    Mock::VerifyAndClearExpectations(&observer_);

    // Sunday midnight the text changes to "Tomorrow".
    EXPECT_CALL(observer_, OnRestrictionScheduleMessageChanged())
        .Times(1)
        .WillOnce(EXPECT_TIME_STR(t.sunday_midnight_utc));
    AdvanceTime(base::Hours(9));  // Sunday 04:00
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_EQ(u"tomorrow", controller_->RestrictionScheduleEndDay());

    // Monday midnight the text changes to "Today".
    EXPECT_CALL(observer_, OnRestrictionScheduleMessageChanged())
        .Times(1)
        .WillOnce(EXPECT_TIME_STR(t.monday_midnight_utc));
    AdvanceTime(base::Days(1));  // Monday 04:00
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_EQ(u"today", controller_->RestrictionScheduleEndDay());
  }
}

class DeviceRestrictionScheduleControllerTestTimeTampering
    : public DeviceRestrictionScheduleControllerTest {
 public:
  // Manually driven in tests.
  void SetUp() override {}
};

// Verify saving highest seen time works.
TEST_F(DeviceRestrictionScheduleControllerTestTimeTampering,
       SavingHighestSeentime) {
  DeviceRestrictionScheduleControllerTest::SetUp();

  const char* time_str = "Tue 29 Oct 2024 14:00";
  SetTime(time_str);
  // Enable the policy so we remember the time.
  UpdatePolicyPref(kPolicyJson);

  // Run any pending timers.
  AdvanceTime(base::TimeDelta());

  EXPECT_EQ(TimeFromString(time_str),
            local_state_.GetTime(
                chromeos::prefs::kDeviceRestrictionScheduleHighestSeenTime));
}

// Verify that the time tampering mechanism doesn't go off if the highest seen
// time wasn't set.
TEST_F(DeviceRestrictionScheduleControllerTestTimeTampering,
       TimeTampering_False) {
  DeviceRestrictionScheduleControllerTest::SetUp();

  SetTime("Tue 29 Oct 2024 14:00");

  // We're outside a restriction schedule and it shouldn't be enabled because
  // there's no tampering with time.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  // Run any pending timers and verify expectations.
  AdvanceTime(base::TimeDelta());
  Mock::VerifyAndClearExpectations(&observer_);
}

// Verify that the time tampering mechanism goes off if the highest seen time is
// more than one day in the future.
TEST_F(DeviceRestrictionScheduleControllerTestTimeTampering,
       TimeTampering_True) {
  local_state_.SetTime(
      chromeos::prefs::kDeviceRestrictionScheduleHighestSeenTime,
      TimeFromString("Thu 31 Oct 2024 14:00"));
  DeviceRestrictionScheduleControllerTest::SetUp();

  SetTime("Tue 29 Oct 2024 14:00");

  // We're outside a restriction schedule, but it should be enabled anyway
  // because we detected tampering with time.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true)).Times(1);
  UpdatePolicyPref(kPolicyJson);

  // Run any pending timers and verify expectations.
  AdvanceTime(base::TimeDelta());
  Mock::VerifyAndClearExpectations(&observer_);
}

// Verify that DST is handled properly (Winter -> Summer).
TEST_F(DeviceRestrictionScheduleControllerTest, HandlingDST_WinterToSummer) {
  // Override the local time zone to fix the DST transitions.
  base::ScopedEnvironmentVariableOverride scoped_timezone(kTZ, "Europe/Berlin");
  // DST starts on Sun, 31 Mar 2024 when the clock moves from 2:00 to 3:00.
  SetTime("Sat 30 Mar 2024 12:00");

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true)).Times(1);
  UpdatePolicyPref(kPolicyJson);
  Mock::VerifyAndClearExpectations(&observer_);

  // Next regular period should start at Mon 06:00.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false))
      .Times(1)
      .WillOnce(EXPECT_TIME_STR("Mon 1 Apr 2024 6:00"));

  AdvanceTime(base::Days(2));
  Mock::VerifyAndClearExpectations(&observer_);
}

// Verify that DST is handled properly (Summer -> Winter).
TEST_F(DeviceRestrictionScheduleControllerTest, HandlingDST_SummerToWinter) {
  // Override the local time zone to fix the DST transitions.
  base::ScopedEnvironmentVariableOverride scoped_timezone(kTZ, "Europe/Berlin");
  // DST ends on Sun, 27 Oct 2024 when the clock moves from 3:00 to 2:00.
  SetTime("Sat 26 Oct 2024 12:00");

  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(true)).Times(1);
  UpdatePolicyPref(kPolicyJson);
  Mock::VerifyAndClearExpectations(&observer_);

  // Next regular period should start at Mon 06:00.
  EXPECT_CALL(observer_, OnRestrictionScheduleStateChanged(false))
      .Times(1)
      .WillOnce(EXPECT_TIME_STR("Mon 28 Oct 2024 6:00"));

  AdvanceTime(base::Days(2));
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
