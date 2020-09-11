// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/feature_status_provider_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/phonehub/fake_connection_manager.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

const char kLocalDeviceBluetoothAddress[] = "01:23:45:67:89:AB";
const char kPhoneBluetoothAddress[] = "23:45:67:89:AB:CD";

multidevice::RemoteDeviceRef CreateLocalDevice(bool supports_phone_hub_client,
                                               bool has_bluetooth_address) {
  multidevice::RemoteDeviceRefBuilder builder;

  builder.SetSoftwareFeatureState(
      multidevice::SoftwareFeature::kPhoneHubClient,
      supports_phone_hub_client
          ? multidevice::SoftwareFeatureState::kSupported
          : multidevice::SoftwareFeatureState::kNotSupported);
  builder.SetBluetoothPublicAddress(
      has_bluetooth_address ? kLocalDeviceBluetoothAddress : std::string());

  return builder.Build();
}

multidevice::RemoteDeviceRef CreatePhoneDevice(
    bool supports_better_together_host,
    bool supports_phone_hub_host,
    bool has_bluetooth_address) {
  multidevice::RemoteDeviceRefBuilder builder;

  builder.SetSoftwareFeatureState(
      multidevice::SoftwareFeature::kBetterTogetherHost,
      supports_better_together_host
          ? multidevice::SoftwareFeatureState::kSupported
          : multidevice::SoftwareFeatureState::kNotSupported);
  builder.SetSoftwareFeatureState(
      multidevice::SoftwareFeature::kPhoneHubHost,
      supports_phone_hub_host
          ? multidevice::SoftwareFeatureState::kSupported
          : multidevice::SoftwareFeatureState::kNotSupported);
  builder.SetBluetoothPublicAddress(
      has_bluetooth_address ? kPhoneBluetoothAddress : std::string());

  return builder.Build();
}

class FakeObserver : public FeatureStatusProvider::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class FeatureStatusProviderImplTest : public testing::Test {
 protected:
  FeatureStatusProviderImplTest() = default;
  FeatureStatusProviderImplTest(const FeatureStatusProviderImplTest&) = delete;
  FeatureStatusProviderImplTest& operator=(
      const FeatureStatusProviderImplTest&) = delete;
  ~FeatureStatusProviderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    is_adapter_present_ = true;
    ON_CALL(*mock_adapter_, IsPresent())
        .WillByDefault(
            Invoke(this, &FeatureStatusProviderImplTest::is_adapter_present));
    ON_CALL(*mock_adapter_, IsPowered())
        .WillByDefault(
            Invoke(this, &FeatureStatusProviderImplTest::is_adapter_powered));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    fake_device_sync_client_.NotifyReady();

    provider_ = std::make_unique<FeatureStatusProviderImpl>(
        &fake_device_sync_client_, &fake_multidevice_setup_client_,
        &fake_connection_manager_);
    provider_->AddObserver(&fake_observer_);
  }

  void SetSyncedDevices(
      const base::Optional<multidevice::RemoteDeviceRef>& local_device,
      const base::Optional<multidevice::RemoteDeviceRef>& phone_device) {
    fake_device_sync_client_.set_local_device_metadata(local_device);

    multidevice::RemoteDeviceRefList synced_devices;
    if (local_device)
      synced_devices.push_back(*local_device);
    if (phone_device)
      synced_devices.push_back(*phone_device);
    fake_device_sync_client_.set_synced_devices(synced_devices);

    fake_device_sync_client_.NotifyNewDevicesSynced();
  }

  void SetEligibleSyncedDevices() {
    SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                       /*has_bluetooth_address=*/true),
                     CreatePhoneDevice(/*supports_better_together_host=*/true,
                                       /*supports_phone_hub_host=*/true,
                                       /*has_bluetooth_address=*/true));
  }

  void SetMultiDeviceState(HostStatus host_status, FeatureState feature_state) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(std::make_pair(
        host_status, CreatePhoneDevice(/*supports_better_together_host=*/true,
                                       /*supports_phone_hub_host=*/true,
                                       /*has_bluetooth_address=*/true)));
    fake_multidevice_setup_client_.SetFeatureState(Feature::kPhoneHub,
                                                   feature_state);
  }

  void SetAdapterPresentState(bool present) {
    if (is_adapter_present_ == present)
      return;

    is_adapter_present_ = present;

    FeatureStatusProviderImpl* impl =
        static_cast<FeatureStatusProviderImpl*>(provider_.get());
    impl->AdapterPresentChanged(mock_adapter_.get(), present);
  }

  void SetAdapterPoweredState(bool powered) {
    if (is_adapter_powered_ == powered)
      return;

    is_adapter_powered_ = powered;

    FeatureStatusProviderImpl* impl =
        static_cast<FeatureStatusProviderImpl*>(provider_.get());
    impl->AdapterPoweredChanged(mock_adapter_.get(), powered);
  }

  void SetConnectionStatus(ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  FeatureStatus GetStatus() const { return provider_->GetStatus(); }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  bool is_adapter_present() { return is_adapter_present_; }
  bool is_adapter_powered() { return is_adapter_powered_; }

  base::test::TaskEnvironment task_environment_;

  device_sync::FakeDeviceSyncClient fake_device_sync_client_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  FakeConnectionManager fake_connection_manager_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  bool is_adapter_present_ = true;
  bool is_adapter_powered_ = true;

  FakeObserver fake_observer_;
  std::unique_ptr<FeatureStatusProvider> provider_;
};

// Tests conditions for kNotEligibleForFeature status, including missing local
// device and/or phone and various missing properties of these devices.
TEST_F(FeatureStatusProviderImplTest, NotEligibleForFeature) {
  SetSyncedDevices(/*local_device=*/base::nullopt,
                   /*phone_device=*/base::nullopt);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/false,
                                     /*has_bluetooth_address=*/false),
                   /*phone_device=*/base::nullopt);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/false),
                   /*phone_device=*/base::nullopt);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/false,
                                     /*has_bluetooth_address=*/true),
                   /*phone_device=*/base::nullopt);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   /*phone_device=*/base::nullopt);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/false,
                                     /*supports_phone_hub_host=*/false,
                                     /*has_bluetooth_address=*/false));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/true,
                                     /*supports_phone_hub_host=*/false,
                                     /*has_bluetooth_address=*/false));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/true,
                                     /*supports_phone_hub_host=*/true,
                                     /*has_bluetooth_address=*/false));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/true,
                                     /*supports_phone_hub_host=*/false,
                                     /*has_bluetooth_address=*/true));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/false,
                                     /*supports_phone_hub_host=*/true,
                                     /*has_bluetooth_address=*/false));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/false,
                                     /*supports_phone_hub_host=*/true,
                                     /*has_bluetooth_address=*/true));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/false,
                                     /*supports_phone_hub_host=*/false,
                                     /*has_bluetooth_address=*/true));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  // Set all properties to true so that there is an eligible phone. Since
  // |fake_multidevice_setup_client_| defaults to kProhibitedByPolicy, the
  // status should still be kNotEligibleForFeature.
  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   CreatePhoneDevice(/*supports_better_together_host=*/true,
                                     /*supports_phone_hub_host=*/true,
                                     /*has_bluetooth_address=*/true));
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, EligiblePhoneButNotSetUp) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kEligibleHostExistsButNoHostSet,
                      FeatureState::kUnavailableNoVerifiedHost);
  EXPECT_EQ(FeatureStatus::kEligiblePhoneButNotSetUp, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, PhoneSelectedAndPendingSetup) {
  SetEligibleSyncedDevices();

  SetMultiDeviceState(
      HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      FeatureState::kUnavailableNoVerifiedHost);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());

  SetMultiDeviceState(HostStatus::kHostSetButNotYetVerified,
                      FeatureState::kUnavailableNoVerifiedHost);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kNotSupportedByPhone);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, Disabled) {
  SetEligibleSyncedDevices();

  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kDisabledByUser);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kUnavailableSuiteDisabled);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kUnavailableTopLevelFeatureDisabled);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, UnavailableBluetoothOff) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser);

  SetAdapterPoweredState(false);
  SetAdapterPresentState(false);
  EXPECT_EQ(FeatureStatus::kUnavailableBluetoothOff, GetStatus());

  SetAdapterPoweredState(true);
  SetAdapterPresentState(false);
  EXPECT_EQ(FeatureStatus::kUnavailableBluetoothOff, GetStatus());

  SetAdapterPoweredState(false);
  SetAdapterPresentState(true);
  EXPECT_EQ(FeatureStatus::kUnavailableBluetoothOff, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, TransitionBetweenAllStatuses) {
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetMultiDeviceState(HostStatus::kEligibleHostExistsButNoHostSet,
                      FeatureState::kUnavailableNoVerifiedHost);
  SetEligibleSyncedDevices();
  EXPECT_EQ(FeatureStatus::kEligiblePhoneButNotSetUp, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetMultiDeviceState(HostStatus::kHostSetButNotYetVerified,
                      FeatureState::kNotSupportedByPhone);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kDisabledByUser);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());

  SetAdapterPoweredState(false);
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser);
  EXPECT_EQ(FeatureStatus::kUnavailableBluetoothOff, GetStatus());
  EXPECT_EQ(4u, GetNumObserverCalls());

  SetAdapterPoweredState(true);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(5u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(6u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kConnected);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnected, GetStatus());
  EXPECT_EQ(7u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kDisconnected);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(8u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, AttemptingConnection) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, AttemptedConnectionSuccessful) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kConnected);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnected, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, AttemptedConnectionFailed) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  SetConnectionStatus(ConnectionManager::Status::kDisconnected);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());
}

}  // namespace phonehub
}  // namespace chromeos
