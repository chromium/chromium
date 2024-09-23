// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/traffic_counters_handler.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace traffic_counters {

namespace {

const char kDecLastResetTime[] = "Fri, 15 December 2023 10:00:00 UTC";

class TrafficCountersHandlerTest : public ::testing::Test {
 public:
  TrafficCountersHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    //  TODO(b/278643115) Remove LoginState dependency.
    LoginState::Initialize();

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());

    feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kTrafficCountersEnabled,
      features::kTrafficCountersForWiFiTesting}, /*disabled_features=*/{});

    helper_ = std::make_unique<NetworkHandlerTestHelper>();
    helper_->AddDefaultProfiles();
    helper_->ResetDevicesAndServices();
    helper_->RegisterPrefs(user_prefs_.registry(), local_state_.registry());

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    helper_->InitializePrefs(&user_prefs_, &local_state_);

    NetworkHandler::Get()->managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY,
        /*userhash=*/std::string(),
        /*network_configs_onc=*/base::Value::List(),
        /*global_network_config=*/base::Value::Dict());

    task_environment_.RunUntilIdle();

    helper_->service_test()->SetTimeGetterForTest(base::BindRepeating(
        [](base::test::TaskEnvironment* env) {
          return env->GetMockClock()->Now();
        },
        &task_environment_));

    TrafficCountersHandler::InitializeForTesting();
    TrafficCountersHandler::Get()->SetTimeGetterForTesting(
      base::BindRepeating(
        [](base::test::TaskEnvironment* env) {
          return env->GetMockClock()->Now();
        },
        &task_environment_));

    SetUpWiFi();
  }

  ~TrafficCountersHandlerTest() override {
    TrafficCountersHandler::Shutdown();
    helper_.reset();
    scoped_user_manager_.reset();
    LoginState::Shutdown();
  }

  base::Time GetTime() { return task_environment_.GetMockClock()->Now(); }

 protected:
  base::Time SetLastResetTimeAndRun(const std::string& time_str) {
    base::Time reset_time = GetTimeFromString(time_str);
    double last_reset_time_ms =
        reset_time.ToDeltaSinceWindowsEpoch().InMilliseconds();
    SetServiceProperty(wifi_path_, shill::kTrafficCounterResetTimeProperty,
                       base::Value(last_reset_time_ms));
    task_environment_.RunUntilIdle();
    RunTrafficCountersHandler();
    return reset_time;
  }

  base::Time GetLastResetTime() {
    double traffic_counter_reset_time_ms =
        helper_
            ->GetServiceDoubleProperty(wifi_path_,
                                       shill::kTrafficCounterResetTimeProperty)
            .value();
    return base::Time::FromDeltaSinceWindowsEpoch(
        base::Milliseconds(traffic_counter_reset_time_ms));
  }

  void SetResetDay(int user_specified_day) {
    NetworkHandler::Get()
        ->network_metadata_store()
        ->SetDayOfTrafficCountersAutoReset(wifi_guid_, user_specified_day);
    task_environment_.RunUntilIdle();
  }

  base::Time AdvanceClockTo(const std::string& time_str) {
    base::Time time = GetTimeFromString(time_str);
    task_environment_.AdvanceClock(
        time.ToDeltaSinceWindowsEpoch() -
        task_environment_.GetMockClock()->Now().ToDeltaSinceWindowsEpoch());
    task_environment_.RunUntilIdle();
    return task_environment_.GetMockClock()->Now();
  }

  void RunTrafficCountersHandler() {
    TrafficCountersHandler::Get()->RunForTesting();
    task_environment_.RunUntilIdle();
  }

  void FastForwardBy(const base::TimeDelta& days) {
    task_environment_.FastForwardBy(days);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TrafficCountersHandler* traffic_counters_handler() {
    return TrafficCountersHandler::Get();
  }

  const std::string& wifi_guid() const { return wifi_guid_; }
  const std::string& wifi_path() const { return wifi_path_; }

 private:
  void SetUpWiFi() {
    ASSERT_TRUE(wifi_path_.empty());
    // By default, NetworkStateTestHelper already adds a WiFi device, so, we
    // do not need to add one here. All that remains to be done is configuring
    // the WiFi service.
    wifi_guid_ = "wifi_guid";
    wifi_path_ = helper_->ConfigureService(
        R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle",
            "SSID": "wifi", "Strength": 100, "AutoConnect": true,
            "WiFi.HiddenSSID": false, "TrafficCounterResetTime": 0})");
    SetServiceProperty(wifi_path_, shill::kStateProperty,
                       base::Value(shill::kStateOnline));
    helper_->profile_test()->AddService(
        NetworkProfileHandler::GetSharedProfilePath(), wifi_path_);
    task_environment_.RunUntilIdle();
  }

  base::Time GetTimeFromString(const std::string& time_str) {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString(time_str.c_str(), &time));
    return time;
  }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    helper_->SetServiceProperty(service_path, key, value);
  }

  // Member order declaration done in a way so that members outlive those that
  // are dependent on them.
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<NetworkHandlerTestHelper> helper_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::string wifi_path_;
  std::string wifi_guid_;
};

}  // namespace

TEST_F(TrafficCountersHandlerTest, GetLastResetTime) {
  // Set the task_environment time to |kDecLastResetTime, ensuring that
  // the base::Now() (simulated "current" time) returns this value.
  AdvanceClockTo(kDecLastResetTime);

  base::Time reset_time =
      SetLastResetTimeAndRun(kDecLastResetTime);
  EXPECT_EQ(GetLastResetTime(), reset_time);
}

TEST_F(TrafficCountersHandlerTest, SetUserSpecifiedResetDay) {
  RunTrafficCountersHandler();
  NetworkHandler::Get()
      ->network_metadata_store()
      ->SetDayOfTrafficCountersAutoReset(wifi_guid(), 13);
  RunUntilIdle();
  const base::Value* reset_day_value =
      NetworkHandler::Get()
          ->network_metadata_store()
          ->GetDayOfTrafficCountersAutoReset(wifi_guid());
  ASSERT_TRUE(reset_day_value && reset_day_value->is_int());
  EXPECT_EQ(reset_day_value->GetInt(), 13);
}

TEST_F(TrafficCountersHandlerTest, FirstOfMonth) {
  AdvanceClockTo(kDecLastResetTime);
  base::Time reset_time =
      SetLastResetTimeAndRun(kDecLastResetTime);
  EXPECT_EQ(GetLastResetTime(), reset_time);

  SetResetDay(1);

  // Advance the clock to Jan 1st, 2024. The counters should get reset.
  base::Time simulated_time1 =
      AdvanceClockTo("Mon, 1 January 2024 07:01:00 UTC");

  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time1);
}

TEST_F(TrafficCountersHandlerTest, EndOfMonth) {
  const std::string last_reset_time_string = "Fri, 1 March 2024 07:01:00 UTC";
  AdvanceClockTo(last_reset_time_string);
  SetLastResetTimeAndRun(last_reset_time_string);

  // Adjust the user specified day to the 15th and ensure that a reset occurs
  // on March 15th.
  SetResetDay(15);
  base::Time simulated_time = AdvanceClockTo("Fri, 15 March 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Adjust the user specified day to the 14th and fast forward by 1 day to
  // March 16th. Ensure that no reset occurs on March 16th.
  SetResetDay(14);
  FastForwardBy(base::Days(1));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Adjust the user specified day to the 31st and Fast forward to Mar 31st.
  // Ensure that a reset occurs.
  SetResetDay(31);
  simulated_time = AdvanceClockTo("Sun, 31 March 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Advance the clock to Apr 30th and ensure auto reset occurs on April
  // 30th since April 31st does not exist.
  simulated_time = AdvanceClockTo("Tue, 30 Apr 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Fast forward by 1 day to May 1st and ensure no auto reset occurs.
  FastForwardBy(base::Days(1));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Advance the clock to May 31st and ensure reset occurs.
  simulated_time = AdvanceClockTo("Fri, 31 May 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, LeapYear) {
  // Set last reset time.
  AdvanceClockTo(kDecLastResetTime);
  base::Time reset_time =
      SetLastResetTimeAndRun(kDecLastResetTime);
  EXPECT_EQ(GetLastResetTime(), reset_time);

  // The first traffic counters reset is expected on Jan 1st.
  SetResetDay(1);
  base::Time simulated_time =
      AdvanceClockTo("Mon, 1 January 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Fast forwarding the date by 15 days to Jan 16th should not affect the
  // last reset time.
  FastForwardBy(base::Days(15));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Fast forwarding the date to Feb 1st should update the last reset time.
  simulated_time = AdvanceClockTo("Thu, 1 February 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 28 days to Feb 29th, ensure February
  // 29th on leap years does not reset traffic counters when the user
  // specified day is the 1st.
  FastForwardBy(base::Days(28));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date to Mar 1st, ensure that two auto
  // traffic counter resets do not occur on the same day.
  // First, ensure traffic counters are reset on March 1st.
  simulated_time = AdvanceClockTo("Fri, 1 March 2024 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Second, run TrafficCountersHandler later on Mar 1st to ensure
  // that another reset doesn't occur.
  FastForwardBy(base::Hours(12));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, NoLeapYear) {
  // Set last reset time.
  const std::string last_reset_time_string =
    "Sat, 15 January 2023 10:00:00 UTC";
  AdvanceClockTo(last_reset_time_string);
  base::Time reset_time =
      SetLastResetTimeAndRun(last_reset_time_string);
  EXPECT_EQ(GetLastResetTime(), reset_time);

  // Set the reset day.
  SetResetDay(31);

  // Adjust the clock to set the "current time". Use Jan 31st, 2023
  // to test whether the functionality is correct on non-leap years.
  base::Time simulated_time =
      AdvanceClockTo("Tue, 31 January 2023 07:01:00 UTC");
  RunTrafficCountersHandler();

  // The first traffic counters reset is expected on Jan 31st.
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date to Feb 28th, ensure traffic
  // counters are reset on February 28th on non-leap years when the user
  // specified day is the 31st.
  simulated_time = AdvanceClockTo("Tue, 28 February 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 1 day to Mar 1st, ensure traffic
  // counters are not reset again.
  FastForwardBy(base::Days(1));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 27 days to March 28th, ensure a
  // traffic counter reset has not occurred.
  FastForwardBy(base::Days(27));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding by 2 days to March 30th, ensure a traffic counter
  // reset has not occurred.
  FastForwardBy(base::Days(2));
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // After fast forwarding the date by 1 day to March 31st, ensure a traffic
  // counter reset occurs.
  simulated_time = AdvanceClockTo("Fri, 31 March 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, ChangeUserSpecifiedDate) {
  // Set last reset time.
  const std::string last_reset_time_string =
    "Sat, 15 January 2023 10:00:00 UTC";
  AdvanceClockTo(last_reset_time_string);
  base::Time reset_time =
      SetLastResetTimeAndRun(last_reset_time_string);
  EXPECT_EQ(GetLastResetTime(), reset_time);

  // Advancing the date to Jan 31st should reset the traffic counters.
  SetResetDay(31);
  base::Time simulated_time =
      AdvanceClockTo("Tue, 31 January 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Change user specified auto reset day.
  SetResetDay(5);
  RunTrafficCountersHandler();

  // After fast forwarding the date to February 5th, ensure traffic counters
  // are reset.
  simulated_time = AdvanceClockTo("Sun, 5 February 2023 07:01:00 UTC");
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, RunFromStart) {
  // Set last reset time.
  const std::string last_reset_time_string =
    "Sat, 15 January 2023 10:00:00 UTC";
  AdvanceClockTo(last_reset_time_string);
  base::Time reset_time =
      SetLastResetTimeAndRun(last_reset_time_string);
  EXPECT_EQ(GetLastResetTime(), reset_time);

  // Set the reset day.
  SetResetDay(1);

  // Start the traffic counters timer.
  traffic_counters_handler()->StartAutoResetForTesting();
  RunUntilIdle();

  // Advance the clock to Jan 2nd, 2024. The timer should run and the
  // counters should get reset.
  base::Time simulated_time =
      AdvanceClockTo("Tue, 2 January 2024 07:01:00 UTC");
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, FastForward40Days) {
  // Set the clock to the specified time and then run traffic counters.
  base::Time simulated_time =
      AdvanceClockTo("Sun, 5 February 2023 07:01:00 UTC");
  RunTrafficCountersHandler();

  // Since no last_reset_time was available in the model, a reset should
  // occur at the time specified above.
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Simulate a device shutdown for 40 days.
  simulated_time = AdvanceClockTo("2023-03-17 07:01:00.000000 UTC");

  // Once device is up and running, run TrafficCountersHandler and confirm
  // that reset date is correct.
  RunTrafficCountersHandler();
  EXPECT_EQ(GetLastResetTime(), simulated_time);
}

TEST_F(TrafficCountersHandlerTest, ChangeResetDayToDayGreaterThanCurrentDay)
{
  // Set last reset time.
  base::Time simulated_time = AdvanceClockTo(kDecLastResetTime);

  // Start the traffic counters timer.
  traffic_counters_handler()->StartAutoResetForTesting();
  RunUntilIdle();

  // Confirm the reset time.
  EXPECT_EQ(GetLastResetTime(), simulated_time);

  // Set the reset day.
  SetResetDay(20);

  // Set simulated time.
  const std::string current_time = "Wed, 20 December 2023 10:00:00 UTC";
  simulated_time = AdvanceClockTo(current_time);
  base::Time reset_time = SetLastResetTimeAndRun(current_time);
  EXPECT_EQ(GetLastResetTime(), reset_time);
}

TEST_F(TrafficCountersHandlerTest, RequestTrafficCounters) {
  base::RunLoop run_loop;
  traffic_counters_handler()->RequestTrafficCounters(
    wifi_path(),
    base::BindOnce(
    [](base::OnceClosure quit_closure,
      std::optional<base::Value> traffic_counters) {
      EXPECT_TRUE(traffic_counters.has_value() && traffic_counters->is_list() &&
                  !traffic_counters->GetList().empty());
      std::move(quit_closure).Run();
    },
    run_loop.QuitClosure()));
    run_loop.Run();
}

TEST_F(TrafficCountersHandlerTest, ResetTrafficCounters) {
  traffic_counters_handler()->ResetTrafficCounters(wifi_path());
  RunUntilIdle();

  base::RunLoop run_loop;
  traffic_counters_handler()->RequestTrafficCounters(
    wifi_path(),
    base::BindOnce(
    [](base::OnceClosure quit_closure,
      std::optional<base::Value> traffic_counters) {
      EXPECT_TRUE(traffic_counters.has_value() && traffic_counters->is_list() &&
                  traffic_counters->GetList().empty());
      std::move(quit_closure).Run();
    },
    run_loop.QuitClosure()));
    run_loop.Run();
}

}  // namespace traffic_counters

}  // namespace ash
