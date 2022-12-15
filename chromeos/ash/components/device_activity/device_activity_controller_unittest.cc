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

}  // namespace

class DeviceActivityControllerTest : public testing::Test {
 public:
  DeviceActivityControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DeviceActivityControllerTest(const DeviceActivityControllerTest&) = delete;
  DeviceActivityControllerTest& operator=(const DeviceActivityControllerTest&) =
      delete;
  ~DeviceActivityControllerTest() override = default;

  TestingPrefServiceSimple* local_state() { return &local_state_; }

 protected:
  // testing::Test:
  void SetUp() override {
    SessionManagerClient::InitializeFake();
    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    DeviceActivityController::RegisterPrefs(local_state()->registry());

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    device_activity_controller_ = std::make_unique<DeviceActivityController>(
        kFakeChromeParameters, local_state(), test_shared_loader_factory_,
        /* first_run_sentinel_time*/ base::Time());
  }

  void TearDown() override { device_activity_controller_.reset(); }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<DeviceActivityController> device_activity_controller_;

  chromeos::system::FakeStatisticsProvider statistics_provider_;
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

  base::Time monthly_ts =
      local_state()->GetTime(prefs::kDeviceActiveLastKnownMonthlyPingTimestamp);
  EXPECT_EQ(monthly_ts, base::Time::UnixEpoch());

  base::Time first_active_ts = local_state()->GetTime(
      prefs::kDeviceActiveLastKnownFirstActivePingTimestamp);
  EXPECT_EQ(first_active_ts, base::Time::UnixEpoch());
}

}  // namespace ash::device_activity
