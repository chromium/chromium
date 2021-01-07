// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 5;

}  // namespace

class MultiDeviceSetupEligibleHostDevicesProviderImplTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  MultiDeviceSetupEligibleHostDevicesProviderImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupEligibleHostDevicesProviderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    use_get_devices_activity_status_ = std::get<0>(GetParam());
    use_connectivity_status_ = std::get<1>(GetParam());
    if (use_get_devices_activity_status_) {
      enabled_features.push_back(
          chromeos::features::kCryptAuthV2DeviceActivityStatus);
    } else {
      disabled_features.push_back(
          chromeos::features::kCryptAuthV2DeviceActivityStatus);
    }
    if (use_connectivity_status_) {
      enabled_features.push_back(
          chromeos::features::kCryptAuthV2DeviceActivityStatusUseConnectivity);
    } else {
      disabled_features.push_back(
          chromeos::features::kCryptAuthV2DeviceActivityStatusUseConnectivity);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);

    provider_ = EligibleHostDevicesProviderImpl::Factory::Create(
        fake_device_sync_client_.get());
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() {
    return fake_device_sync_client_.get();
  }

  multidevice::RemoteDeviceRefList& test_devices() { return test_devices_; }

  EligibleHostDevicesProvider* provider() { return provider_.get(); }

  void SetBitsOnTestDevices() {
    // Devices 0, 1, and 2 are supported.
    GetMutableRemoteDevice(test_devices()[0])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[1])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[2])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;

    // Device 3 is enabled.
    GetMutableRemoteDevice(test_devices()[3])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kEnabled;

    // Device 4 is not supported.
    GetMutableRemoteDevice(test_devices()[4])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kNotSupported;
  }

  bool use_get_devices_activity_status() const {
    return use_get_devices_activity_status_;
  }

  bool use_connectivity_status() const { return use_connectivity_status_; }

 private:
  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

  std::unique_ptr<EligibleHostDevicesProvider> provider_;

  bool use_get_devices_activity_status_;
  bool use_connectivity_status_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupEligibleHostDevicesProviderImplTest);
};

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest, Empty) {
  EXPECT_TRUE(provider()->GetEligibleHostDevices().empty());
}

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest, NoEligibleDevices) {
  GetMutableRemoteDevice(test_devices()[0])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kNotSupported;
  GetMutableRemoteDevice(test_devices()[1])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kNotSupported;

  multidevice::RemoteDeviceRefList devices{test_devices()[0],
                                           test_devices()[1]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  EXPECT_TRUE(provider()->GetEligibleHostDevices().empty());
}

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest,
       SupportedAndEnabled) {
  SetBitsOnTestDevices();

  GetMutableRemoteDevice(test_devices()[0])->last_update_time_millis = 1999;
  GetMutableRemoteDevice(test_devices()[1])->last_update_time_millis = 25;
  GetMutableRemoteDevice(test_devices()[2])->last_update_time_millis = 2525;
  GetMutableRemoteDevice(test_devices()[3])->last_update_time_millis = 500;
  GetMutableRemoteDevice(test_devices()[4])->last_update_time_millis = 1000;

  multidevice::RemoteDeviceRefList devices{test_devices()[0], test_devices()[1],
                                           test_devices()[2], test_devices()[3],
                                           test_devices()[4]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  multidevice::RemoteDeviceRefList eligible_devices =
      provider()->GetEligibleHostDevices();
  EXPECT_EQ(4u, eligible_devices.size());
  EXPECT_EQ(test_devices()[2], eligible_devices[0]);
  EXPECT_EQ(test_devices()[0], eligible_devices[1]);
  EXPECT_EQ(test_devices()[3], eligible_devices[2]);
  EXPECT_EQ(test_devices()[1], eligible_devices[3]);
}

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest,
       GetDevicesActivityStatus) {
  SetBitsOnTestDevices();

  GetMutableRemoteDevice(test_devices()[0])->last_update_time_millis = 1;
  GetMutableRemoteDevice(test_devices()[1])->last_update_time_millis = 25;
  GetMutableRemoteDevice(test_devices()[2])->last_update_time_millis = 10;
  GetMutableRemoteDevice(test_devices()[3])->last_update_time_millis = 100;
  GetMutableRemoteDevice(test_devices()[4])->last_update_time_millis = 10000;

  multidevice::RemoteDeviceRefList devices{test_devices()[0], test_devices()[1],
                                           test_devices()[2], test_devices()[3],
                                           test_devices()[4]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  // Set current time so that no devices are filtered out based on their last
  // activity time
  base::subtle::ScopedTimeClockOverrides time_now_override(
      []() { return base::Time::FromTimeT(20000); }, nullptr, nullptr);

  std::vector<device_sync::mojom::DeviceActivityStatusPtr>
      device_activity_statuses;
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[0].instance_id(), base::Time::FromTimeT(50),
          cryptauthv2::ConnectivityStatus::ONLINE));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[1].instance_id(), base::Time::FromTimeT(100),
          cryptauthv2::ConnectivityStatus::OFFLINE));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[2].instance_id(), base::Time::FromTimeT(200),
          cryptauthv2::ConnectivityStatus::ONLINE));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[3].instance_id(), base::Time::FromTimeT(50),
          cryptauthv2::ConnectivityStatus::ONLINE));
  if (use_get_devices_activity_status()) {
    fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
        device_sync::mojom::NetworkRequestResult::kSuccess,
        std::move(device_activity_statuses));
  }

  multidevice::DeviceWithConnectivityStatusList eligible_active_devices =
      provider()->GetEligibleActiveHostDevices();
  EXPECT_EQ(4u, eligible_active_devices.size());

  // Verify sorting.
  if (use_get_devices_activity_status()) {
    if (use_connectivity_status()) {
      EXPECT_EQ(test_devices()[2], eligible_active_devices[0].remote_device);
      EXPECT_EQ(test_devices()[3], eligible_active_devices[1].remote_device);
      EXPECT_EQ(test_devices()[0], eligible_active_devices[2].remote_device);
      EXPECT_EQ(test_devices()[1], eligible_active_devices[3].remote_device);
    } else {
      // Ignore online/offline statuses during sorting.
      EXPECT_EQ(test_devices()[2], eligible_active_devices[0].remote_device);
      EXPECT_EQ(test_devices()[1], eligible_active_devices[1].remote_device);
      EXPECT_EQ(test_devices()[3], eligible_active_devices[2].remote_device);
      EXPECT_EQ(test_devices()[0], eligible_active_devices[3].remote_device);
    }
  } else {
    multidevice::RemoteDeviceRefList eligible_devices =
        provider()->GetEligibleHostDevices();
    EXPECT_EQ(4u, eligible_devices.size());
    EXPECT_EQ(test_devices()[3], eligible_devices[0]);
    EXPECT_EQ(test_devices()[1], eligible_devices[1]);
    EXPECT_EQ(test_devices()[2], eligible_devices[2]);
    EXPECT_EQ(test_devices()[0], eligible_devices[3]);

    for (size_t i = 0; i < 4; i++) {
      EXPECT_EQ(eligible_devices[i], eligible_active_devices[i].remote_device);
    }
  }

  // Verify connectivity statuses.
  if (use_get_devices_activity_status() && use_connectivity_status()) {
    EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
              eligible_active_devices[0].connectivity_status);
    EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
              eligible_active_devices[1].connectivity_status);
    EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
              eligible_active_devices[2].connectivity_status);
    EXPECT_EQ(cryptauthv2::ConnectivityStatus::OFFLINE,
              eligible_active_devices[3].connectivity_status);
  } else {
    for (size_t i = 0; i < 4; i++) {
      EXPECT_EQ(cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY,
                eligible_active_devices[i].connectivity_status);
    }
  }
}

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest,
       GetDevicesActivityStatusRemovesInactiveDevices) {
  if (!use_get_devices_activity_status()) {
    return;
  }

  SetBitsOnTestDevices();

  base::subtle::ScopedTimeClockOverrides time_now_override(
      []() {
        return base::Time() +
               EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays +
               base::TimeDelta::FromDays(1000);
      },
      nullptr, nullptr);

  multidevice::RemoteDeviceRefList devices{test_devices()[0], test_devices()[1],
                                           test_devices()[2], test_devices()[3],
                                           test_devices()[4]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  std::vector<device_sync::mojom::DeviceActivityStatusPtr>
      device_activity_statuses;
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[0].instance_id(), base::Time(),
          cryptauthv2::ConnectivityStatus::ONLINE));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[1].instance_id(),
          base::Time::Now() -
              EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays -
              base::TimeDelta::FromDays(1),
          cryptauthv2::ConnectivityStatus::OFFLINE));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[2].instance_id(),
          base::Time::Now() -
              EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays -
              base::TimeDelta::FromDays(10),
          cryptauthv2::ConnectivityStatus::ONLINE));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[3].instance_id(),
          base::Time::Now() -
              EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays,
          cryptauthv2::ConnectivityStatus::ONLINE));

  fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      std::move(device_activity_statuses));

  multidevice::DeviceWithConnectivityStatusList eligible_active_devices =
      provider()->GetEligibleActiveHostDevices();

  EXPECT_EQ(2u, eligible_active_devices.size());
  EXPECT_EQ(test_devices()[3], eligible_active_devices[0].remote_device);
  EXPECT_EQ(test_devices()[0], eligible_active_devices[1].remote_device);
}

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest,
       GetDevicesActivityStatusFailedRequest) {
  if (!use_get_devices_activity_status()) {
    return;
  }

  SetBitsOnTestDevices();
  GetMutableRemoteDevice(test_devices()[0])->last_update_time_millis = 5;
  GetMutableRemoteDevice(test_devices()[1])->last_update_time_millis = 4;
  GetMutableRemoteDevice(test_devices()[2])->last_update_time_millis = 3;
  GetMutableRemoteDevice(test_devices()[3])->last_update_time_millis = 2;
  GetMutableRemoteDevice(test_devices()[4])->last_update_time_millis = 1;

  multidevice::RemoteDeviceRefList devices{test_devices()[0], test_devices()[1],
                                           test_devices()[2], test_devices()[3],
                                           test_devices()[4]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();
  fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
      device_sync::mojom::NetworkRequestResult::kInternalServerError,
      base::nullopt);

  multidevice::DeviceWithConnectivityStatusList eligible_active_devices =
      provider()->GetEligibleActiveHostDevices();
  multidevice::RemoteDeviceRefList eligible_devices =
      provider()->GetEligibleHostDevices();
  EXPECT_EQ(test_devices()[0], eligible_active_devices[0].remote_device);
  EXPECT_EQ(test_devices()[1], eligible_active_devices[1].remote_device);
  EXPECT_EQ(test_devices()[2], eligible_active_devices[2].remote_device);
  EXPECT_EQ(test_devices()[3], eligible_active_devices[3].remote_device);
  EXPECT_EQ(test_devices()[0], eligible_devices[0]);
  EXPECT_EQ(test_devices()[1], eligible_devices[1]);
  EXPECT_EQ(test_devices()[2], eligible_devices[2]);
  EXPECT_EQ(test_devices()[3], eligible_devices[3]);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MultiDeviceSetupEligibleHostDevicesProviderImplTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace multidevice_setup

}  // namespace chromeos
