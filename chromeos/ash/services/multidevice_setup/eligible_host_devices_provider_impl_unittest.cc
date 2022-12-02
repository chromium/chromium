// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/eligible_host_devices_provider_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 6;

}  // namespace

class MultiDeviceSetupEligibleHostDevicesProviderImplTest
    : public ::testing::TestWithParam<std::tuple<bool, bool, bool, bool>> {
 public:
  MultiDeviceSetupEligibleHostDevicesProviderImplTest(
      const MultiDeviceSetupEligibleHostDevicesProviderImplTest&) = delete;
  MultiDeviceSetupEligibleHostDevicesProviderImplTest& operator=(
      const MultiDeviceSetupEligibleHostDevicesProviderImplTest&) = delete;

 protected:
  MultiDeviceSetupEligibleHostDevicesProviderImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupEligibleHostDevicesProviderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    use_get_devices_activity_status_ = std::get<0>(GetParam());
    use_connectivity_status_ = std::get<1>(GetParam());
    always_use_active_eligible_devices_ = std::get<2>(GetParam());
    use_last_activity_time_to_dedup_ = std::get<3>(GetParam());
    if (use_get_devices_activity_status_) {
      enabled_features.push_back(features::kCryptAuthV2DeviceActivityStatus);
    } else {
      disabled_features.push_back(features::kCryptAuthV2DeviceActivityStatus);
    }
    if (use_connectivity_status_) {
      enabled_features.push_back(
          features::kCryptAuthV2DeviceActivityStatusUseConnectivity);
    } else {
      disabled_features.push_back(
          features::kCryptAuthV2DeviceActivityStatusUseConnectivity);
    }
    if (always_use_active_eligible_devices_) {
      enabled_features.push_back(
          features::kCryptAuthV2AlwaysUseActiveEligibleHosts);
    } else {
      disabled_features.push_back(
          features::kCryptAuthV2AlwaysUseActiveEligibleHosts);
    }
    if (use_last_activity_time_to_dedup_) {
      enabled_features.push_back(
          features::kCryptAuthV2DedupDeviceLastActivityTime);
    } else {
      disabled_features.push_back(
          features::kCryptAuthV2DedupDeviceLastActivityTime);
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
    // Devices 0, 1, 2, and 3 are supported.
    GetMutableRemoteDevice(test_devices()[0])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[1])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[2])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[3])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;

    // Device 4 is enabled.
    GetMutableRemoteDevice(test_devices()[4])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kEnabled;

    // Device 5 is not supported.
    GetMutableRemoteDevice(test_devices()[5])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kNotSupported;
  }

  bool use_get_devices_activity_status() const {
    return use_get_devices_activity_status_;
  }

  bool use_connectivity_status() const { return use_connectivity_status_; }

  // When the flags is enabled, GetEligibleHostDevices() is the same as
  // GetEligibleActiveHostDevices() without the connectivity status.
  bool always_use_active_eligible_devices() const {
    return always_use_active_eligible_devices_;
  }

  // When the flag is enabled, only one of devices with same last_activity_time
  // will be kept.
  bool use_last_activity_time_to_dedup() const {
    return use_last_activity_time_to_dedup_;
  }

 private:
  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

  std::unique_ptr<EligibleHostDevicesProvider> provider_;

  bool use_get_devices_activity_status_;
  bool use_connectivity_status_;
  bool always_use_active_eligible_devices_;
  bool use_last_activity_time_to_dedup_;

  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest, Sorting) {
  SetBitsOnTestDevices();

  GetMutableRemoteDevice(test_devices()[0])->last_update_time_millis = 1;
  GetMutableRemoteDevice(test_devices()[1])->last_update_time_millis = 25;
  GetMutableRemoteDevice(test_devices()[2])->last_update_time_millis = 10;
  GetMutableRemoteDevice(test_devices()[3])->last_update_time_millis = 100;
  GetMutableRemoteDevice(test_devices()[4])->last_update_time_millis = 1000;
  GetMutableRemoteDevice(test_devices()[5])->last_update_time_millis = 10000;

  multidevice::RemoteDeviceRefList devices{
      test_devices()[0], test_devices()[1], test_devices()[2],
      test_devices()[3], test_devices()[4], test_devices()[5]};
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
          test_devices()[0].instance_id(),
          /*last_activity_time=*/base::Time::FromTimeT(50),
          cryptauthv2::ConnectivityStatus::ONLINE,
          /*last_update_time=*/base::Time::FromTimeT(4)));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[1].instance_id(),
          /*last_activity_time=*/base::Time::FromTimeT(100),
          cryptauthv2::ConnectivityStatus::OFFLINE,
          /*last_update_time=*/base::Time::FromTimeT(2)));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[2].instance_id(),
          /*last_activity_time=*/base::Time::FromTimeT(200),
          cryptauthv2::ConnectivityStatus::ONLINE,
          /*last_update_time=*/base::Time::FromTimeT(1)));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[3].instance_id(),
          /*last_activity_time=*/base::Time::FromTimeT(50),
          cryptauthv2::ConnectivityStatus::ONLINE,
          /*last_update_time=*/base::Time::FromTimeT(4)));
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[4].instance_id(),
          /*last_activity_time=*/base::Time::FromTimeT(50),
          cryptauthv2::ConnectivityStatus::ONLINE,
          /*last_update_time=*/base::Time::FromTimeT(3)));
  if (use_get_devices_activity_status()) {
    fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
        device_sync::mojom::NetworkRequestResult::kSuccess,
        std::move(device_activity_statuses));
  }

  multidevice::RemoteDeviceRefList eligible_devices =
      provider()->GetEligibleHostDevices();

  multidevice::DeviceWithConnectivityStatusList eligible_active_devices =
      provider()->GetEligibleActiveHostDevices();

  if (use_get_devices_activity_status()) {
    // Verify sorting by online/offline status (if flag enabled), then by
    // |last_activity_time|, then by |last_update_time| (from
    // GetDevicesActivityStatus), then by |last_update_time_millis| (from
    // RemoteDevice).
    if (use_connectivity_status()) {
      if (use_last_activity_time_to_dedup()) {
        // If the kCryptAuthV2DedupDeviceLastActivityTime flag is enabled, only
        // the first one of devices sharing same last_activity_time will be
        // kept, sorted by online/offline status, last_update_time, and
        // last_update_time_millis.
        EXPECT_EQ(3u, eligible_active_devices.size());
        EXPECT_EQ(test_devices()[2], eligible_active_devices[0].remote_device);
        EXPECT_EQ(test_devices()[3], eligible_active_devices[1].remote_device);
        EXPECT_EQ(test_devices()[1], eligible_active_devices[2].remote_device);
      } else {
        EXPECT_EQ(5u, eligible_active_devices.size());
        EXPECT_EQ(test_devices()[2], eligible_active_devices[0].remote_device);
        EXPECT_EQ(test_devices()[3], eligible_active_devices[1].remote_device);
        EXPECT_EQ(test_devices()[0], eligible_active_devices[2].remote_device);
        EXPECT_EQ(test_devices()[4], eligible_active_devices[3].remote_device);
        EXPECT_EQ(test_devices()[1], eligible_active_devices[4].remote_device);

        // Verify connectivity statuses.
        EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
                  eligible_active_devices[0].connectivity_status);
        EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
                  eligible_active_devices[1].connectivity_status);
        EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
                  eligible_active_devices[2].connectivity_status);
        EXPECT_EQ(cryptauthv2::ConnectivityStatus::ONLINE,
                  eligible_active_devices[3].connectivity_status);
        EXPECT_EQ(cryptauthv2::ConnectivityStatus::OFFLINE,
                  eligible_active_devices[4].connectivity_status);
      }
    } else {
      if (use_last_activity_time_to_dedup()) {
        // If the kCryptAuthV2DedupDeviceLastActivityTime flag is enabled, only
        // the first one of devices sharing same last_activity_time will be
        // kept, sorted by last_update_time and last_update_time_millis.
        EXPECT_EQ(3u, eligible_active_devices.size());
        EXPECT_EQ(test_devices()[2], eligible_active_devices[0].remote_device);
        EXPECT_EQ(test_devices()[1], eligible_active_devices[1].remote_device);
        EXPECT_EQ(test_devices()[3], eligible_active_devices[2].remote_device);
      } else {
        EXPECT_EQ(5u, eligible_active_devices.size());
        // Ignore online/offline statuses during sorting.
        EXPECT_EQ(test_devices()[2], eligible_active_devices[0].remote_device);
        EXPECT_EQ(test_devices()[1], eligible_active_devices[1].remote_device);
        EXPECT_EQ(test_devices()[3], eligible_active_devices[2].remote_device);
        EXPECT_EQ(test_devices()[0], eligible_active_devices[3].remote_device);
        EXPECT_EQ(test_devices()[4], eligible_active_devices[4].remote_device);
      }
    }
  } else {
    // Sorting solely based on RemoteDevice's |last_update_time_millis|.
    EXPECT_EQ(5u, eligible_devices.size());
    EXPECT_EQ(test_devices()[4], eligible_devices[0]);
    EXPECT_EQ(test_devices()[3], eligible_devices[1]);
    EXPECT_EQ(test_devices()[1], eligible_devices[2]);
    EXPECT_EQ(test_devices()[2], eligible_devices[3]);
    EXPECT_EQ(test_devices()[0], eligible_devices[4]);

    for (size_t i = 0; i < eligible_active_devices.size(); i++) {
      EXPECT_EQ(eligible_devices[i], eligible_active_devices[i].remote_device);
    }
  }

  if (always_use_active_eligible_devices()) {
    for (size_t i = 0; i < eligible_active_devices.size(); i++) {
      EXPECT_EQ(eligible_devices[i], eligible_active_devices[i].remote_device);
    }
  } else {
    EXPECT_EQ(5u, eligible_devices.size());
  }

  // Verify connectivity statuses.
  if (!use_get_devices_activity_status() || !use_connectivity_status()) {
    for (const auto& eligible_active_device : eligible_active_devices) {
      EXPECT_EQ(cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY,
                eligible_active_device.connectivity_status);
    }
  }
}

TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest,
       RemoveStaleDevices) {
  if (!use_get_devices_activity_status()) {
    return;
  }

  SetBitsOnTestDevices();

  base::subtle::ScopedTimeClockOverrides time_now_override(
      []() {
        return base::Time() +
               EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays +
               base::Days(1000);
      },
      nullptr, nullptr);

  multidevice::RemoteDeviceRefList devices{
      test_devices()[0], test_devices()[1], test_devices()[2],
      test_devices()[3], test_devices()[4], test_devices()[5]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  std::vector<device_sync::mojom::DeviceActivityStatusPtr>
      device_activity_statuses;

  // Do not filter out based on unset timestamps or based on connectivity
  // status.
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[0].instance_id(), /*last_activity_time=*/base::Time(),
          cryptauthv2::ConnectivityStatus::OFFLINE,
          /*last_update_time=*/base::Time()));

  // Filter out based on DeviceActivityStatus's |last_activity_time|.
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[1].instance_id(),
          /*last_activity_time=*/base::Time::Now() -
              EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays -
              base::Days(1),
          cryptauthv2::ConnectivityStatus::ONLINE,
          /*last_update_time=*/base::Time::Now()));

  // Filter out based on DeviceActivityStatus's |last_update_time|.
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[2].instance_id(),
          /*last_activity_time=*/base::Time::Now(),
          cryptauthv2::ConnectivityStatus::ONLINE,
          /*last_update_time=*/base::Time::Now() -
              EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays -
              base::Days(1)));

  // Do not filter out; times within threhhold
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[3].instance_id(),
          /*last_activity_time=*/base::Time::Now() -
              EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays,
          cryptauthv2::ConnectivityStatus::ONLINE,
          /*last_update_time=*/base::Time::Now() -
              EligibleHostDevicesProviderImpl::kInactiveDeviceThresholdInDays));

  // Do not filter out test_devices()[4]; no device activity status returned.

  fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      std::move(device_activity_statuses));

  multidevice::DeviceWithConnectivityStatusList eligible_active_devices =
      provider()->GetEligibleActiveHostDevices();
  EXPECT_EQ(3u, eligible_active_devices.size());
  EXPECT_EQ(test_devices()[3], eligible_active_devices[0].remote_device);
  EXPECT_EQ(test_devices()[0], eligible_active_devices[1].remote_device);
  EXPECT_EQ(test_devices()[4], eligible_active_devices[2].remote_device);

  if (always_use_active_eligible_devices()) {
    multidevice::RemoteDeviceRefList eligible_devices =
        provider()->GetEligibleHostDevices();
    for (size_t i = 0; i < eligible_active_devices.size(); i++) {
      EXPECT_EQ(eligible_devices[i], eligible_active_devices[i].remote_device);
    }
  }
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
      absl::nullopt);

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
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace multidevice_setup

}  // namespace ash
