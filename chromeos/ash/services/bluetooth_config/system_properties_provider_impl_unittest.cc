// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/system_properties_provider_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/fake_fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/fake_system_properties_observer.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

mojom::PairedBluetoothDevicePropertiesPtr GenerateStubPairedDeviceProperties() {
  auto device_properties = mojom::BluetoothDeviceProperties::New();
  device_properties->id = "id";
  device_properties->public_name = u"name";
  device_properties->device_type = mojom::DeviceType::kUnknown;
  device_properties->audio_capability =
      mojom::AudioOutputCapability::kNotCapableOfAudioOutput;
  device_properties->connection_state =
      mojom::DeviceConnectionState::kNotConnected;

  mojom::PairedBluetoothDevicePropertiesPtr paired_properties =
      mojom::PairedBluetoothDeviceProperties::New();
  paired_properties->device_properties = std::move(device_properties);
  return paired_properties;
}

mojom::PairedBluetoothDevicePropertiesPtr
GenerateFastPairableDeviceProperties() {
  auto bluetooth_device_properties =
      bluetooth_config::mojom::BluetoothDeviceProperties::New();
  bluetooth_device_properties->address = "address";
  auto properties =
      bluetooth_config::mojom::PairedBluetoothDeviceProperties::New();
  properties->device_properties = std::move(bluetooth_device_properties);
  properties->fast_pairable_device_pairing_state =
      bluetooth_config::mojom::FastPairableDevicePairingState::kReady;
  properties->nickname = "nickname";

  return properties;
}

}  // namespace

class SystemPropertiesProviderImplTest : public testing::Test {
 protected:
  SystemPropertiesProviderImplTest() = default;
  SystemPropertiesProviderImplTest(const SystemPropertiesProviderImplTest&) =
      delete;
  SystemPropertiesProviderImplTest& operator=(
      const SystemPropertiesProviderImplTest&) = delete;
  ~SystemPropertiesProviderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Enable feature.
    feature_list.InitAndEnableFeature(
        features::kFastPairDevicesBluetoothSettings);
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
    session_manager_ = std::make_unique<session_manager::SessionManager>();

    provider_ = std::make_unique<SystemPropertiesProviderImpl>(
        &fake_adapter_state_controller_, &fake_device_cache_,
        &fake_fast_pair_delegate_);
  }

  void SetSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
    provider_->FlushForTesting();
  }

  void SetPairedDevices(
      const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
          paired_devices) {
    std::vector<mojom::PairedBluetoothDevicePropertiesPtr> copy;
    for (const auto& paired_device : paired_devices)
      copy.push_back(paired_device.Clone());

    fake_device_cache_.SetPairedDevices(std::move(copy));
    provider_->FlushForTesting();
  }

  const user_manager::User* LogIn(const std::string email) {
    const AccountId account_id = AccountId::FromUserEmail(email);
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);

    // Create a session in SessionManager. This will also login the user in
    // UserManager.
    session_manager_->CreateSession(user->GetAccountId(), user->username_hash(),
                                    /*is_child=*/false);
    session_manager_->SessionStarted();

    // Logging in doesn't set the user in UserManager as the active user if
    // there already is an active user, do so manually.
    SwitchActiveUser(account_id);
    return user;
  }

  void SwitchActiveUser(const AccountId& account_id) {
    fake_user_manager_->SwitchActiveUser(account_id);
    SetSessionState(session_manager::SessionState::ACTIVE);
  }

  void SetSessionState(session_manager::SessionState session_state) {
    session_manager_->SetSessionState(session_state);
    provider_->FlushForTesting();
  }

  std::unique_ptr<FakeSystemPropertiesObserver> Observe() {
    auto observer = std::make_unique<FakeSystemPropertiesObserver>();
    provider_->Observe(observer->GeneratePendingRemote());
    provider_->FlushForTesting();
    return observer;
  }

  void SetFastPairableDeviceProperties(
      std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
          fast_pairable_device_properties) {
    fake_fast_pair_delegate_.SetFastPairableDeviceProperties(
        fast_pairable_device_properties);
    provider_->FlushForTesting();
  }

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  GetFastPairableDeviceProperties() {
    return fake_fast_pair_delegate_.GetFastPairableDeviceProperties();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  FakeAdapterStateController fake_adapter_state_controller_;
  FakeDeviceCache fake_device_cache_{&fake_adapter_state_controller_};
  FakeFastPairDelegate fake_fast_pair_delegate_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  raw_ptr<user_manager::FakeUserManager, DanglingUntriaged> fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList feature_list;

  std::unique_ptr<SystemPropertiesProviderImpl> provider_;
};

TEST_F(SystemPropertiesProviderImplTest, SystemStateChanges) {
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();

  // Once Observe() is called, the observer should immediately receive one
  // update with the current state.
  ASSERT_EQ(1u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kEnabled,
            observer->received_properties_list()[0]->system_state);

  // Change the state to kDisabled and verify that the observer was notified.
  SetSystemState(mojom::BluetoothSystemState::kDisabled);
  ASSERT_EQ(2u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kDisabled,
            observer->received_properties_list()[1]->system_state);

  // Change the state to kUnavailable and verify that the observer was notified.
  SetSystemState(mojom::BluetoothSystemState::kUnavailable);
  ASSERT_EQ(3u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothSystemState::kUnavailable,
            observer->received_properties_list()[2]->system_state);
}

TEST_F(SystemPropertiesProviderImplTest, PairedDeviceChanges) {
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();

  // Once Observe() is called, the observer should immediately receive one
  // update with the current state.
  ASSERT_EQ(1u, observer->received_properties_list().size());
  EXPECT_TRUE(observer->received_properties_list()[0]->paired_devices.empty());

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices;
  paired_devices.push_back(GenerateStubPairedDeviceProperties());

  // Add a paired device and verify that the observer was notified.
  SetPairedDevices(paired_devices);
  ASSERT_EQ(2u, observer->received_properties_list().size());
  EXPECT_EQ(1u, observer->received_properties_list()[1]->paired_devices.size());

  // Add another paired device and verify that the observer was notified.
  paired_devices.push_back(GenerateStubPairedDeviceProperties());
  SetPairedDevices(paired_devices);
  ASSERT_EQ(3u, observer->received_properties_list().size());
  EXPECT_EQ(2u, observer->received_properties_list()[2]->paired_devices.size());
}

TEST_F(SystemPropertiesProviderImplTest, FastPairableDeviceChanges) {
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();

  // Once Observe() is called, the observer should immediately receive one
  // update with the current state.
  ASSERT_EQ(1u, observer->received_properties_list().size());
  EXPECT_TRUE(
      observer->received_properties_list()[0]->fast_pairable_devices.empty());

  // Add a fast pairable device and verify that the observer was notified.
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> fast_pairable_devices;
  fast_pairable_devices.push_back(GenerateFastPairableDeviceProperties());
  SetFastPairableDeviceProperties(fast_pairable_devices);
  ASSERT_EQ(2u, observer->received_properties_list().size());
  EXPECT_EQ(
      1u,
      observer->received_properties_list()[1]->fast_pairable_devices.size());
  EXPECT_EQ("address",
            GetFastPairableDeviceProperties()[0]->device_properties->address);

  // Add another fast pairable device and verify that the observer was notified.
  fast_pairable_devices.push_back(GenerateFastPairableDeviceProperties());
  SetFastPairableDeviceProperties(fast_pairable_devices);
  ASSERT_EQ(3u, observer->received_properties_list().size());
  EXPECT_EQ(
      2u,
      observer->received_properties_list()[2]->fast_pairable_devices.size());
  EXPECT_EQ("address",
            GetFastPairableDeviceProperties()[1]->device_properties->address);
}

TEST_F(SystemPropertiesProviderImplTest, ModificationStateChanges) {
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();

  // Once Observe() is called, the observer should immediately receive one
  // update with the current state. Bluetooth is modifiable before any user logs
  // in.
  ASSERT_EQ(1u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCanModifyBluetooth,
            observer->received_properties_list()[0]->modification_state);

  // Log in as the first user. They should be able to modify Bluetooth.
  const user_manager::User* user1 = LogIn("email1@example.com");
  ASSERT_EQ(2u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCanModifyBluetooth,
            observer->received_properties_list()[1]->modification_state);

  // Lock the screen. They should not be able to modify Bluetooth.
  SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(3u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCannotModifyBluetooth,
            observer->received_properties_list()[2]->modification_state);

  // Log in as a second user. They should not be able to modify Bluetooth.
  LogIn("email2@example.com");
  ASSERT_EQ(4u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCannotModifyBluetooth,
            observer->received_properties_list()[3]->modification_state);

  // Lock the screen. They should not be able to modify Bluetooth.
  SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(5u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCannotModifyBluetooth,
            observer->received_properties_list()[4]->modification_state);

  // Switch to the first user again. They should be able to modify Bluetooth.
  SwitchActiveUser(user1->GetAccountId());
  ASSERT_EQ(6u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCanModifyBluetooth,
            observer->received_properties_list()[5]->modification_state);
}

TEST_F(SystemPropertiesProviderImplTest, DisconnectToStopObserving) {
  // The initial properties list should have been received.
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();
  ASSERT_EQ(1u, observer->received_properties_list().size());

  // Disconnect the Mojo pipe; this should stop observing.
  observer->DisconnectMojoPipe();

  // Change the state to kDisabled; the observer should not be notified since it
  // is no longer connected.
  SetSystemState(mojom::BluetoothSystemState::kDisabled);
  EXPECT_EQ(1u, observer->received_properties_list().size());
}

}  // namespace ash::bluetooth_config
