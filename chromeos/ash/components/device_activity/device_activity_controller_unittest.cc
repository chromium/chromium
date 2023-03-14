// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/device_activity_controller.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::device_activity {

namespace {

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

// Number of times to retry before failing to report any device actives.
constexpr int kNumberOfRetriesBeforeFail = 120;

}  // namespace

class DeviceActivityControllerTest : public testing::Test {
 public:
  // Create a fake callback that returns successful TimeDelta
  // time since oobe file written of 1 minute.
  DeviceActivityControllerTest()
      : DeviceActivityControllerTest(
            base::BindRepeating([]() { return base::Minutes(1); })) {}
  explicit DeviceActivityControllerTest(
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback)
      : oobe_completed_callback_(std::move(check_oobe_completed_callback)),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DeviceActivityControllerTest(const DeviceActivityControllerTest&) = delete;
  DeviceActivityControllerTest& operator=(const DeviceActivityControllerTest&) =
      delete;
  ~DeviceActivityControllerTest() override = default;

  TestingPrefServiceSimple* local_state() { return &local_state_; }

  DeviceActivityController* GetDeviceActivityController() {
    return device_activity_controller_.get();
  }

 protected:
  // testing::Test:
  void SetUp() override {
    SessionManagerClient::InitializeFake();
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    DeviceActivityController::RegisterPrefs(local_state()->registry());

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    device_activity_controller_ = std::make_unique<DeviceActivityController>(
        kFakeChromeParameters, local_state(), test_shared_loader_factory_,
        /* first_run_sentinel_time*/ base::Time(), oobe_completed_callback_);

    // Continue execution of the Start method since there is no start up delay
    // in the post delay task.
    RunUntilIdle();
  }

  void TearDown() override { device_activity_controller_.reset(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  // The repeating callback is owned by this test class.
  base::RepeatingCallback<base::TimeDelta()> oobe_completed_callback_;

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<DeviceActivityController> device_activity_controller_;

  system::FakeStatisticsProvider statistics_provider_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(DeviceActivityControllerTest,
       CheckDeviceActivityControllerSingletonInitialized) {
  EXPECT_NE(DeviceActivityController::Get(), nullptr);
}

TEST_F(DeviceActivityControllerTest,
       CheckLocalStatePingTimestampsInitializedToUnixEpoch) {
  base::Time daily_ts =
      local_state()->GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp);
  EXPECT_EQ(daily_ts, base::Time::UnixEpoch());

  base::Time cohort_monthly_ts = local_state()->GetTime(
      prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp);
  EXPECT_EQ(cohort_monthly_ts, base::Time::UnixEpoch());

  base::Time observation_monthly_ts = local_state()->GetTime(
      prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp);
  EXPECT_EQ(observation_monthly_ts, base::Time::UnixEpoch());
}

TEST_F(DeviceActivityControllerTest,
       CheckOobeCompletedFileSuccessfullyWritten) {
  // Validate that there were no retries with the |oobe_completed_callback_|
  // being successful.
  EXPECT_EQ(
      GetDeviceActivityController()->GetRetryOobeCompletedCountForTesting(), 0);
}

class DeviceActivityControllerOobeFileRetryFailsTest
    : public DeviceActivityControllerTest {
 public:
  // Create a fake callback that retry oobe check kNumberOfRetriesBeforeFail
  // times and fails. It fails by returning empty TimeDelta object.
  DeviceActivityControllerOobeFileRetryFailsTest()
      : DeviceActivityControllerTest(
            base::BindRepeating([]() { return base::TimeDelta(); })) {}
  DeviceActivityControllerOobeFileRetryFailsTest(
      const DeviceActivityControllerOobeFileRetryFailsTest&) = delete;
  DeviceActivityControllerOobeFileRetryFailsTest& operator=(
      const DeviceActivityControllerOobeFileRetryFailsTest&) = delete;
  ~DeviceActivityControllerOobeFileRetryFailsTest() override = default;
};

TEST_F(DeviceActivityControllerOobeFileRetryFailsTest,
       StopReportingIfOobeFileFailedToBeWritten) {
  base::OneShotTimer* timer =
      GetDeviceActivityController()->GetOobeCompletedTimerForTesting();

  for (int retry_count = 1; retry_count <= kNumberOfRetriesBeforeFail;
       retry_count++) {
    // Attempt at reading file failed.
    EXPECT_EQ(
        GetDeviceActivityController()->GetRetryOobeCompletedCountForTesting(),
        retry_count);

    // Retry, which happens after 1 hour on "real" devices.
    timer->FireNow();
    RunUntilIdle();
  }

  EXPECT_EQ(
      GetDeviceActivityController()->GetRetryOobeCompletedCountForTesting(),
      kNumberOfRetriesBeforeFail);
}

}  // namespace ash::device_activity
