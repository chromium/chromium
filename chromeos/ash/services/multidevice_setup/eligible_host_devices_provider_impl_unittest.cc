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

const size_t kNumTestDevices = 7;

const char kBluetoothAddress0[] = "01:01:01:01:01:00";
const char kBluetoothAddress1[] = "01:01:01:01:01:01";
const char kBluetoothAddress2[] = "01:01:01:01:01:02";
const char kBluetoothAddress3[] = "01:01:01:01:01:03";
const char kBluetoothAddress4[] = "01:01:01:01:01:04";
const char kBluetoothAddress5[] = "01:01:01:01:01:05";

}  // namespace

class MultiDeviceSetupEligibleHostDevicesProviderImplTest
    : public ::testing::TestWithParam<std::tuple<bool, bool, bool>>,
      public EligibleHostDevicesProvider::Observer {
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
    use_last_activity_time_to_dedup_ = std::get<2>(GetParam());
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
    provider_->AddObserver(this);
  }

  void TearDown() override { provider_->RemoveObserver(this); }

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
    GetMutableRemoteDevice(test_devices()[0])->bluetooth_public_address =
        kBluetoothAddress0;
    GetMutableRemoteDevice(test_devices()[1])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[1])->bluetooth_public_address =
        kBluetoothAddress1;
    GetMutableRemoteDevice(test_devices()[2])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[2])->bluetooth_public_address =
        kBluetoothAddress2;
    GetMutableRemoteDevice(test_devices()[3])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[3])->bluetooth_public_address =
        kBluetoothAddress3;

    // Device 4 is enabled.
    GetMutableRemoteDevice(test_devices()[4])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kEnabled;
    GetMutableRemoteDevice(test_devices()[4])->bluetooth_public_address =
        kBluetoothAddress4;

    // Device 5 is not supported.
    GetMutableRemoteDevice(test_devices()[5])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kNotSupported;
    GetMutableRemoteDevice(test_devices()[5])->bluetooth_public_address =
        kBluetoothAddress5;

    // Device 6 is supported, and has the same bluetooth address as Device 0.
    GetMutableRemoteDevice(test_devices()[6])
        ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
        multidevice::SoftwareFeatureState::kSupported;
    GetMutableRemoteDevice(test_devices()[6])->bluetooth_public_address =
        kBluetoothAddress0;
  }

  // EligibleHostDevicesProvider::Observer:
  void OnEligibleDevicesSynced() override {
    notified_eligible_devices_synced_ = true;
  }

  bool use_get_devices_activity_status() const {
    return use_get_devices_activity_status_;
  }

  bool use_connectivity_status() const { return use_connectivity_status_; }

  // When the flag is enabled, only one of devices with same last_activity_time
  // will be kept.
  bool use_last_activity_time_to_dedup() const {
    return use_last_activity_time_to_dedup_;
  }

  bool notified_eligible_devices_synced() const {
    return notified_eligible_devices_synced_;
  }

 private:
  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

  std::unique_ptr<EligibleHostDevicesProvider> provider_;

  bool use_get_devices_activity_status_;
  bool use_connectivity_status_;
  bool use_last_activity_time_to_dedup_;
  bool notified_eligible_devices_synced_ = false;

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

  if (use_get_devices_activity_status()) {
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
            cryptauthv2::ConnectivityStatus::ONLINE,
            /*last_update_time=*/base::Time::FromTimeT(2)));
    fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
        device_sync::mojom::NetworkRequestResult::kSuccess,
        std::move(device_activity_statuses));
  }

  EXPECT_TRUE(provider()->GetEligibleHostDevices().empty());
  EXPECT_TRUE(notified_eligible_devices_synced());
}

// Regression test for b/207089877
TEST_P(MultiDeviceSetupEligibleHostDevicesProviderImplTest,
       NoEligibleDevices_NoDeviceId) {
  GetMutableRemoteDevice(test_devices()[0])->instance_id = std::string();
  GetMutableRemoteDevice(test_devices()[1])->instance_id = std::string();

  multidevice::RemoteDeviceRefList devices{test_devices()[0],
                                           test_devices()[1]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  if (use_get_devices_activity_status()) {
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
            cryptauthv2::ConnectivityStatus::ONLINE,
            /*last_update_time=*/base::Time::FromTimeT(2)));
    fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
        device_sync::mojom::NetworkRequestResult::kSuccess,
        std::move(device_activity_statuses));
  }

  EXPECT_TRUE(provider()->GetEligibleHostDevices().empty());
  EXPECT_TRUE(notified_eligible_devices_synced());
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

  for (size_t i = 0; i < eligible_active_devices.size(); i++) {
    EXPECT_EQ(eligible_devices[i], eligible_active_devices[i].remote_device);
  }

  // Verify connectivity statuses.
  if (!use_get_devices_activity_status() || !use_connectivity_status()) {
    for (const auto& eligible_active_device : eligible_active_devices) {
      EXPECT_EQ(cryptauthv2::ConnectivityStatus::UNKNOWN_CONNECTIVITY,
                eligible_active_device.connectivity_status);
    }
  }

  EXPECT_TRUE(notified_eligible_devices_synced());
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

  multidevice::RemoteDeviceRefList devices{test_devices()[0], test_devices()[1],
                                           test_devices()[2], test_devices()[3],
                                           test_devices()[4], test_devices()[5],
                                           test_devices()[6]};
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

  // Filter out because match of public bluetooth address with Device 0.
  device_activity_statuses.emplace_back(
      device_sync::mojom::DeviceActivityStatus::New(
          test_devices()[6].instance_id(), /*last_activity_time=*/base::Time(),
          cryptauthv2::ConnectivityStatus::OFFLINE,
          /*last_update_time=*/base::Time()));

  fake_device_sync_client()->InvokePendingGetDevicesActivityStatusCallback(
      device_sync::mojom::NetworkRequestResult::kSuccess,
      std::move(device_activity_statuses));

  multidevice::DeviceWithConnectivityStatusList eligible_active_devices =
      provider()->GetEligibleActiveHostDevices();
  EXPECT_EQ(3u, eligible_active_devices.size());
  EXPECT_EQ(test_devices()[3], eligible_active_devices[0].remote_device);
  EXPECT_EQ(test_devices()[0], eligible_active_devices[1].remote_device);
  EXPECT_EQ(test_devices()[4], eligible_active_devices[2].remote_device);

  multidevice::RemoteDeviceRefList eligible_devices =
      provider()->GetEligibleHostDevices();
  for (size_t i = 0; i < eligible_active_devices.size(); i++) {
    EXPECT_EQ(eligible_devices[i], eligible_active_devices[i].remote_device);
  }

  EXPECT_TRUE(notified_eligible_devices_synced());
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
      std::nullopt);

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
  EXPECT_TRUE(notified_eligible_devices_synced());
}

INSTANTIATE_TEST_SUITE_P(All,
                         MultiDeviceSetupEligibleHostDevicesProviderImplTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace multidevice_setup

}  // namespace ash
