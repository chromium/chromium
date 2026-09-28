// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/system_properties_provider_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_cache.h"
#include "chromeos/ash/services/bluetooth_config/fake_system_properties_observer.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

constexpr AccountId::Literal kUser1AccountId =
    AccountId::Literal::FromUserEmailGaiaId("email1@example.com",
                                            GaiaId::Literal("fakegaia1"));
constexpr AccountId::Literal kUser2AccountId =
    AccountId::Literal::FromUserEmailGaiaId("email2@example.com",
                                            GaiaId::Literal("fakegaia2"));

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
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
    ASSERT_TRUE(
        user_session_test_environment_->AddRegularUser(kUser1AccountId));
    ASSERT_TRUE(
        user_session_test_environment_->AddRegularUser(kUser2AccountId));

    provider_ = std::make_unique<SystemPropertiesProviderImpl>(
        &fake_adapter_state_controller_, &fake_device_cache_);
  }

  void TearDown() override {
    provider_.reset();
    user_session_test_environment_.reset();
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

  void LogIn(const AccountId& account_id) {
    user_session_test_environment_->LogIn(account_id);
    provider_->FlushForTesting();
  }

  void SwitchActiveUser(const AccountId& account_id) {
    session_manager::SessionManager::Get()->SwitchActiveSession(account_id);
    SetSessionState(session_manager::SessionState::ACTIVE);
  }

  void SetSessionState(session_manager::SessionState session_state) {
    session_manager::SessionManager::Get()->SetSessionState(session_state);
    provider_->FlushForTesting();
  }

  std::unique_ptr<FakeSystemPropertiesObserver> Observe() {
    auto observer = std::make_unique<FakeSystemPropertiesObserver>();
    provider_->Observe(observer->GeneratePendingRemote());
    provider_->FlushForTesting();
    return observer;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;

  FakeAdapterStateController fake_adapter_state_controller_;
  FakeDeviceCache fake_device_cache_{&fake_adapter_state_controller_};
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

TEST_F(SystemPropertiesProviderImplTest, ModificationStateChanges) {
  std::unique_ptr<FakeSystemPropertiesObserver> observer = Observe();

  // Once Observe() is called, the observer should immediately receive one
  // update with the current state. Bluetooth is modifiable before any user logs
  // in.
  ASSERT_EQ(1u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCanModifyBluetooth,
            observer->received_properties_list()[0]->modification_state);

  // Log in as the first user. They should be able to modify Bluetooth.
  LogIn(kUser1AccountId);
  ASSERT_EQ(2u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCanModifyBluetooth,
            observer->received_properties_list()[1]->modification_state);

  // Lock the screen. They should not be able to modify Bluetooth.
  SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(3u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCannotModifyBluetooth,
            observer->received_properties_list()[2]->modification_state);

  // Log in as a second user. The session is active again with the first user
  // still active, so Bluetooth is modifiable.
  LogIn(kUser2AccountId);
  ASSERT_EQ(4u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCanModifyBluetooth,
            observer->received_properties_list()[3]->modification_state);

  // Lock the screen, then switch to the second user. They should not be able
  // to modify Bluetooth.
  SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(5u, observer->received_properties_list().size());
  SwitchActiveUser(kUser2AccountId);
  ASSERT_EQ(6u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCannotModifyBluetooth,
            observer->received_properties_list()[5]->modification_state);

  // Lock the screen. They should not be able to modify Bluetooth.
  SetSessionState(session_manager::SessionState::LOCKED);
  ASSERT_EQ(7u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCannotModifyBluetooth,
            observer->received_properties_list()[6]->modification_state);

  // Switch to the first user again. They should be able to modify Bluetooth.
  SwitchActiveUser(kUser1AccountId);
  ASSERT_EQ(8u, observer->received_properties_list().size());
  EXPECT_EQ(mojom::BluetoothModificationState::kCanModifyBluetooth,
            observer->received_properties_list()[7]->modification_state);
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
