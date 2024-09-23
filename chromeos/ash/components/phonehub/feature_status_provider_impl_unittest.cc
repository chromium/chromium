// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/feature_status_provider_impl.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {

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

multidevice::RemoteDeviceRef CreatePhoneDeviceWithUniqueInstanceId(
    bool supports_better_together_host,
    bool supports_phone_hub_host,
    bool has_bluetooth_address,
    std::string instance_id) {
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
  builder.SetInstanceId(instance_id);
  return builder.Build();
}

class FakeObserver : public FeatureStatusProvider::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }
  size_t eligible_host_calls() const { return eligible_host_calls_; }

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override { ++num_calls_; }
  void OnEligiblePhoneHubHostFound(
      const multidevice::RemoteDeviceRefList device) override {
    ++eligible_host_calls_;
  }

 private:
  size_t num_calls_ = 0;
  size_t eligible_host_calls_ = 0;
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

    session_manager_ = std::make_unique<session_manager::SessionManager>();
    fake_power_manager_client_ =
        std::make_unique<chromeos::FakePowerManagerClient>();
    PhoneHubStructuredMetricsLogger::RegisterPrefs(pref_service_.registry());
    phone_hub_structured_metrics_logger_ =
        std::make_unique<PhoneHubStructuredMetricsLogger>(&pref_service_);
    provider_ = std::make_unique<FeatureStatusProviderImpl>(
        &fake_device_sync_client_, &fake_multidevice_setup_client_,
        &fake_connection_manager_, session_manager_.get(),
        fake_power_manager_client_.get(),
        phone_hub_structured_metrics_logger_.get());
    provider_->AddObserver(&fake_observer_);
  }

  void SetSyncedDevices(
      const std::optional<multidevice::RemoteDeviceRef>& local_device,
      const std::vector<std::optional<multidevice::RemoteDeviceRef>>
          phone_devices) {
    fake_device_sync_client_.set_local_device_metadata(local_device);

    multidevice::RemoteDeviceRefList synced_devices;
    if (local_device) {
      synced_devices.push_back(*local_device);
    }
    for (const auto& phone_device : phone_devices) {
      if (phone_device) {
        synced_devices.push_back(*phone_device);
      }
    }
    fake_device_sync_client_.set_synced_devices(synced_devices);

    fake_device_sync_client_.NotifyNewDevicesSynced();
  }

  void SetEligibleSyncedDevices() {
    SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                       /*has_bluetooth_address=*/true),
                     {CreatePhoneDevice(/*supports_better_together_host=*/true,
                                        /*supports_phone_hub_host=*/true,
                                        /*has_bluetooth_address=*/true)});
  }

  void SetMultiDeviceState(HostStatus host_status,
                           FeatureState feature_state,
                           bool supports_better_together_host,
                           bool supports_phone_hub,
                           bool has_bluetooth_address) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(std::make_pair(
        host_status,
        CreatePhoneDevice(supports_better_together_host, supports_phone_hub,
                          has_bluetooth_address)));
    fake_multidevice_setup_client_.SetFeatureState(Feature::kPhoneHub,
                                                   feature_state);
  }

  void SetHostStatusWithDevice(
      HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(host_status, host_device));
  }

  void SetAdapterPresentState(bool present) {
    if (is_adapter_present_ == present) {
      return;
    }

    is_adapter_present_ = present;

    FeatureStatusProviderImpl* impl =
        static_cast<FeatureStatusProviderImpl*>(provider_.get());
    impl->AdapterPresentChanged(mock_adapter_.get(), present);
  }

  void SetAdapterPoweredState(bool powered) {
    if (is_adapter_powered_ == powered) {
      return;
    }

    is_adapter_powered_ = powered;

    FeatureStatusProviderImpl* impl =
        static_cast<FeatureStatusProviderImpl*>(provider_.get());
    impl->AdapterPoweredChanged(mock_adapter_.get(), powered);
  }

  void SetConnectionStatus(secure_channel::ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  void SetFeatureState(FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(Feature::kPhoneHub,
                                                   feature_state);
  }

  FeatureStatus GetStatus() const { return provider_->GetStatus(); }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }
  size_t GetNumEligibleHostObserverCalls() const {
    return fake_observer_.eligible_host_calls();
  }

  session_manager::SessionManager* session_manager() {
    return session_manager_.get();
  }

 private:
  bool is_adapter_present() { return is_adapter_present_; }
  bool is_adapter_powered() { return is_adapter_powered_; }

  base::test::TaskEnvironment task_environment_;

  device_sync::FakeDeviceSyncClient fake_device_sync_client_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  secure_channel::FakeConnectionManager fake_connection_manager_;

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  bool is_adapter_present_ = true;
  bool is_adapter_powered_ = true;

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PhoneHubStructuredMetricsLogger>
      phone_hub_structured_metrics_logger_;
  FakeObserver fake_observer_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<chromeos::FakePowerManagerClient> fake_power_manager_client_;
  std::unique_ptr<FeatureStatusProvider> provider_;
};

// Tests conditions for kNotEligibleForFeature status, including missing local
// device and/or phone and various missing properties of these devices.
TEST_F(FeatureStatusProviderImplTest, NotEligibleForFeature) {
  SetSyncedDevices(/*local_device=*/std::nullopt,
                   /*phone_devices=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/false,
                                     /*has_bluetooth_address=*/false),
                   /*phone_devices=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/false),
                   /*phone_devices=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/false,
                                     /*has_bluetooth_address=*/true),
                   /*phone_devices=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   /*phone_device=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/false,
                                      /*supports_phone_hub_host=*/false,
                                      /*has_bluetooth_address=*/false)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/true,
                                      /*supports_phone_hub_host=*/false,
                                      /*has_bluetooth_address=*/false)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/true,
                                      /*supports_phone_hub_host=*/true,
                                      /*has_bluetooth_address=*/false)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/true,
                                      /*supports_phone_hub_host=*/false,
                                      /*has_bluetooth_address=*/true)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/false,
                                      /*supports_phone_hub_host=*/true,
                                      /*has_bluetooth_address=*/false)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/false,
                                      /*supports_phone_hub_host=*/true,
                                      /*has_bluetooth_address=*/true)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/false,
                                      /*supports_phone_hub_host=*/false,
                                      /*has_bluetooth_address=*/true)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  // Set all properties to true so that there is an eligible phone. Since
  // |fake_multidevice_setup_client_| defaults to kProhibitedByPolicy, the
  // status should still be kNotEligibleForFeature.
  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/true,
                                      /*supports_phone_hub_host=*/true,
                                      /*has_bluetooth_address=*/true)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  // Simulate having multiple phones available that are both not eligible.
  // We want to have a null host so that it simulates searching through all
  // synced devices to find an available host. Since all phones are not
  // eligible, expect that we return kNotEligibleForFeature.
  SetFeatureState(FeatureState::kEnabledByUser);
  SetHostStatusWithDevice(HostStatus::kEligibleHostExistsButNoHostSet,
                          /*host_device=*/std::nullopt);
  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/false,
                                      /*supports_phone_hub_host=*/false,
                                      /*has_bluetooth_address=*/true),
                    CreatePhoneDevice(/*supports_better_together_host=*/false,
                                      /*supports_phone_hub_host=*/false,
                                      /*has_bluetooth_address=*/true)});
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, EligiblePhoneButNotSetUp) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(
      HostStatus::kEligibleHostExistsButNoHostSet,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/true,
      /*supports_phone_hub=*/true,
      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kEligiblePhoneButNotSetUp, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, NoEligiblePhones) {
  SetMultiDeviceState(HostStatus::kNoEligibleHosts,
                      FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, MultiPhoneEligibility) {
  // There is an eligible phone but the current host phone is not eligible.
  // Expect kNotEligibleForFeature to return.
  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {CreatePhoneDevice(/*supports_better_together_host=*/true,
                                      /*supports_phone_hub_host=*/true,
                                      /*has_bluetooth_address=*/true),
                    CreatePhoneDevice(/*supports_better_together_host=*/false,
                                      /*supports_phone_hub_host=*/false,
                                      /*has_bluetooth_address=*/true)});
  SetMultiDeviceState(
      HostStatus::kEligibleHostExistsButNoHostSet,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/true,
      /*supports_phone_hub=*/false,
      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetMultiDeviceState(
      HostStatus::kEligibleHostExistsButNoHostSet,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/false,
      /*supports_phone_hub=*/true,
      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetMultiDeviceState(
      HostStatus::kEligibleHostExistsButNoHostSet,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/true,
      /*supports_phone_hub=*/true,
      /*has_bluetooth_address=*/false);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  // Simulate no host device connected and expect to detect one eligible host.
  SetHostStatusWithDevice(HostStatus::kEligibleHostExistsButNoHostSet,
                          /*host_device=*/std::nullopt);
  EXPECT_EQ(FeatureStatus::kEligiblePhoneButNotSetUp, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, PhoneSelectedAndPendingSetup) {
  SetEligibleSyncedDevices();

  SetMultiDeviceState(
      HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/true,
      /*supports_phone_hub=*/true,
      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());

  SetMultiDeviceState(
      HostStatus::kHostSetButNotYetVerified,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/true,
      /*supports_phone_hub=*/true,
      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kNotSupportedByPhone,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, Disabled) {
  SetEligibleSyncedDevices();

  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kDisabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kUnavailableSuiteDisabled,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kUnavailableTopLevelFeatureDisabled,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());
}

TEST_F(FeatureStatusProviderImplTest, UnavailableBluetoothOff) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);

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

  SetMultiDeviceState(HostStatus::kNoEligibleHosts,
                      FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());

  SetMultiDeviceState(
      HostStatus::kEligibleHostExistsButNoHostSet,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/true,
      /*supports_phone_hub=*/true,
      /*has_bluetooth_address=*/true);
  SetEligibleSyncedDevices();
  EXPECT_EQ(FeatureStatus::kEligiblePhoneButNotSetUp, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetMultiDeviceState(HostStatus::kHostSetButNotYetVerified,
                      FeatureState::kNotSupportedByPhone,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kPhoneSelectedAndPendingSetup, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kDisabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());

  SetAdapterPoweredState(false);
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kUnavailableBluetoothOff, GetStatus());
  EXPECT_EQ(4u, GetNumObserverCalls());

  SetAdapterPoweredState(true);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(5u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(6u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnected, GetStatus());
  EXPECT_EQ(7u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(8u, GetNumObserverCalls());

  // Simulate suspended state, which includes screen locked and power suspended.
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(FeatureStatus::kLockOrSuspended, GetStatus());
  EXPECT_EQ(9u, GetNumObserverCalls());

  // Simulate user unlocks the device.
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(10u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, AttemptingConnection) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, AttemptedConnectionSuccessful) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnected, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, AttemptedConnectionFailed) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kEnabledAndConnecting, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, LockScreenStatusUpdate) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Simulate lock screen displayed.
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(FeatureStatus::kLockOrSuspended, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Simulate user unlocks the device.
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, HandlePowerSuspend) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  // Simulate an imminent power suspend event.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_EQ(FeatureStatus::kLockOrSuspended, GetStatus());
  EXPECT_EQ(2u, GetNumObserverCalls());

  // Simulate a power suspend done event.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_EQ(FeatureStatus::kEnabledButDisconnected, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());
}

TEST_F(FeatureStatusProviderImplTest, EligiblePhoneHubHostsFound) {
  SetMultiDeviceState(
      HostStatus::kEligibleHostExistsButNoHostSet,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*supports_better_together_host=*/true,
      /*supports_phone_hub=*/true,
      /*has_bluetooth_address=*/true);

  // Create devices with different instance Id's.
  multidevice::RemoteDeviceRef device_1 = CreatePhoneDeviceWithUniqueInstanceId(
      /*supports_better_together_host=*/true,
      /*supports_phone_hub_host=*/true,
      /*has_bluetooth_address=*/true, "AAA");
  multidevice::RemoteDeviceRef device_2 = CreatePhoneDeviceWithUniqueInstanceId(
      /*supports_better_together_host=*/true,
      /*supports_phone_hub_host=*/true,
      /*has_bluetooth_address=*/true, "AAB");

  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {device_1, device_2});
  EXPECT_EQ(FeatureStatus::kEligiblePhoneButNotSetUp, GetStatus());
  EXPECT_EQ(GetNumEligibleHostObserverCalls(), 1u);

  multidevice::RemoteDeviceRef device_3 = CreatePhoneDeviceWithUniqueInstanceId(
      /*supports_better_together_host=*/true,
      /*supports_phone_hub_host=*/true,
      /*has_bluetooth_address=*/true, "AAC");
  SetSyncedDevices(CreateLocalDevice(/*supports_phone_hub_client=*/true,
                                     /*has_bluetooth_address=*/true),
                   {device_1, device_2, device_3});

  EXPECT_EQ(FeatureStatus::kEligiblePhoneButNotSetUp, GetStatus());
  EXPECT_EQ(GetNumEligibleHostObserverCalls(), 2u);
}

TEST_F(FeatureStatusProviderImplTest, NotSupportedByChromebook) {
  SetEligibleSyncedDevices();
  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kNotSupportedByChromebook,
                      /*supports_better_together_host=*/true,
                      /*supports_phone_hub=*/true,
                      /*has_bluetooth_address=*/true);

  // When the multidevice feature state is kNotSupportedByChromebook, then the
  // Phonehub status is kNotEligibleForFeature.
  EXPECT_EQ(FeatureStatus::kNotEligibleForFeature, GetStatus());
}

}  // namespace ash::phonehub
