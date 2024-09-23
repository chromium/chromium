// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/bluetooth_power_controller_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

constexpr char kUser1Email[] = "user1@bluetooth";

}  // namespace

class BluetoothPowerControllerImplTest : public testing::Test {
 protected:
  BluetoothPowerControllerImplTest() = default;
  BluetoothPowerControllerImplTest(const BluetoothPowerControllerImplTest&) =
      delete;
  BluetoothPowerControllerImplTest& operator=(
      const BluetoothPowerControllerImplTest&) = delete;
  ~BluetoothPowerControllerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    BluetoothPowerControllerImpl::RegisterLocalStatePrefs(
        local_state()->registry());
    BluetoothPowerControllerImpl::RegisterProfilePrefs(
        active_user_prefs()->registry());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }

  void Init() {
    bluetooth_power_controller_ =
        std::make_unique<BluetoothPowerControllerImpl>(
            &fake_adapter_state_controller_);
    bluetooth_power_controller_->SetPrefs(/*logged_in_profile_prefs=*/nullptr,
                                          local_state());
  }

  void AddUserSession(const std::string& display_email,
                      bool is_user_kiosk = false,
                      bool is_new_profile = false) {
    const AccountId account_id = AccountId::FromUserEmail(display_email);
    const user_manager::User* user;
    if (is_user_kiosk) {
      user = fake_user_manager_->AddKioskAppUser(account_id);
    } else {
      user = fake_user_manager_->AddUser(account_id);
    }
    fake_user_manager_->SetIsCurrentUserNew(is_new_profile);

    // Create a session in SessionManager. This will also login the user in
    // UserManager.
    session_manager_->CreateSession(user->GetAccountId(), user->username_hash(),
                                    /*is_child=*/false);
    session_manager_->SessionStarted();

    // Logging in doesn't set the user in UserManager as the active user if
    // there already is an active user, do so manually.
    fake_user_manager_->SwitchActiveUser(account_id);

    bluetooth_power_controller_->SetPrefs(&active_user_prefs_, local_state());
  }

  void SetBluetoothSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
  }

  mojom::BluetoothSystemState GetAdapterState() const {
    return fake_adapter_state_controller_.GetAdapterState();
  }

  void SetBluetoothEnabledState(bool enabled) {
    bluetooth_power_controller_->SetBluetoothEnabledState(enabled);
  }

  void SetBluetoothEnabledWithoutPersistence() {
    bluetooth_power_controller_->SetBluetoothEnabledWithoutPersistence();
  }

  void SetBluetoothHidDetectionInactive(bool is_using_bluetooth) {
    bluetooth_power_controller_->SetBluetoothHidDetectionInactive(
        is_using_bluetooth);
  }

  sync_preferences::TestingPrefServiceSyncable* local_state() {
    return &local_state_;
  }
  sync_preferences::TestingPrefServiceSyncable* active_user_prefs() {
    return &active_user_prefs_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  raw_ptr<user_manager::FakeUserManager, DanglingUntriaged> fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  sync_preferences::TestingPrefServiceSyncable active_user_prefs_;
  sync_preferences::TestingPrefServiceSyncable local_state_;

  FakeAdapterStateController fake_adapter_state_controller_;

  std::unique_ptr<BluetoothPowerController> bluetooth_power_controller_;
};

// Tests toggling Bluetooth setting on and off.
TEST_F(BluetoothPowerControllerImplTest, ToggleBluetoothEnabled) {
  // Makes sure we start with Bluetooth power on.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kEnabled);

  Init();

  // By default, the local state gets set to enabled.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabled);

  // Toggling Bluetooth off/on when there is no user session should affect
  // local state prefs.
  SetBluetoothEnabledState(false);
  EXPECT_FALSE(
      local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  SetBluetoothEnabledState(true);
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  // Toggling Bluetooth off/on when there is user session should affect
  // user prefs.
  AddUserSession(kUser1Email);
  EXPECT_TRUE(
      active_user_prefs()->GetBoolean(prefs::kUserBluetoothAdapterEnabled));

  SetBluetoothEnabledState(false);
  EXPECT_FALSE(
      active_user_prefs()->GetBoolean(prefs::kUserBluetoothAdapterEnabled));

  // Local state prefs should remain unchanged.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  SetBluetoothEnabledState(true);
  EXPECT_TRUE(
      active_user_prefs()->GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
}

// Tests how BluetoothPowerController applies the local state pref when
// the pref hasn't been set before.
TEST_F(BluetoothPowerControllerImplTest, ApplyBluetoothLocalStatePrefDefault) {
  // Makes sure pref hasn't been set before.
  local_state()->RemoveUserPref(prefs::kSystemBluetoothAdapterEnabled);
  EXPECT_TRUE(local_state()
                  ->FindPreference(prefs::kSystemBluetoothAdapterEnabled)
                  ->IsDefaultValue());

  Init();

  // Pref should now contain the current Bluetooth adapter state (on).
  EXPECT_FALSE(local_state()
                   ->FindPreference(prefs::kSystemBluetoothAdapterEnabled)
                   ->IsDefaultValue());
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
}

// Tests how BluetoothPowerController applies the local state pref when
// the pref has been set before.
TEST_F(BluetoothPowerControllerImplTest, ApplyBluetoothLocalStatePrefOn) {
  // Set the pref to true.
  local_state()->SetBoolean(prefs::kSystemBluetoothAdapterEnabled, true);
  EXPECT_FALSE(local_state()
                   ->FindPreference(prefs::kSystemBluetoothAdapterEnabled)
                   ->IsDefaultValue());

  // Start with Bluetooth power off.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabled);

  Init();

  // Bluetooth power setting should be applied (on), and pref value unchanged.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabling);
}

// Tests how BluetoothPowerController applies the local state pref when
// the pref has been set before but the adapter is initially unavailable. This
// simulates the edge case where the device just starts up, the adapter is
// unavailable, then later becomes available.
TEST_F(BluetoothPowerControllerImplTest,
       ApplyBluetoothLocalStatePrefOn_InitialAdapterUnavailable) {
  // Set the pref to true.
  local_state()->SetBoolean(prefs::kSystemBluetoothAdapterEnabled, true);
  EXPECT_FALSE(local_state()
                   ->FindPreference(prefs::kSystemBluetoothAdapterEnabled)
                   ->IsDefaultValue());

  // Start with the adapter unavailable.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kUnavailable);
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kUnavailable);

  Init();

  // The pref value should be unchanged and the adapter still unavailable.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kUnavailable);

  // Set the adapter as available but disabled.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);

  // Bluetooth power setting should be applied (on).
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabling);
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
}

// Tests how BluetoothPowerController applies the user pref when
// the pref hasn't been set before.
TEST_F(BluetoothPowerControllerImplTest, ApplyBluetoothPrimaryUserPrefDefault) {
  // Start with Bluetooth power on.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kEnabled);

  Init();

  // Makes sure pref hasn't been set before.
  EXPECT_TRUE(active_user_prefs()
                  ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
                  ->IsDefaultValue());
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabled);

  AddUserSession(kUser1Email);

  // Pref should now contain the current Bluetooth adapter state (on).
  EXPECT_FALSE(active_user_prefs()
                   ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
                   ->IsDefaultValue());
  EXPECT_TRUE(
      active_user_prefs()->GetBoolean(prefs::kUserBluetoothAdapterEnabled));
}

// Tests how BluetoothPowerController applies the user pref when
// the pref hasn't been set before, and it's a first-login user.
TEST_F(BluetoothPowerControllerImplTest,
       ApplyBluetoothPrimaryUserPrefDefaultNew) {
  // Start with Bluetooth power off.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);

  Init();

  // Makes sure pref hasn't been set before.
  EXPECT_TRUE(active_user_prefs()
                  ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
                  ->IsDefaultValue());
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabled);

  AddUserSession(kUser1Email, /*is_user_kiosk=*/false, /*is_new_profile=*/true);

  // Pref should be set to true for first-login users, and this will also
  // trigger the Bluetooth power on.
  EXPECT_FALSE(active_user_prefs()
                   ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
                   ->IsDefaultValue());
  EXPECT_TRUE(
      active_user_prefs()->GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabling);
}

// Tests how BluetoothPowerController applies the user pref when
// the pref hasn't been set before, but not a regular user (e.g. kiosk).
TEST_F(BluetoothPowerControllerImplTest, ApplyBluetoothKioskUserPrefDefault) {
  // Start with Bluetooth power off.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);

  Init();

  // Makes sure pref hasn't been set before.
  EXPECT_TRUE(active_user_prefs()
                  ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
                  ->IsDefaultValue());
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabled);

  AddUserSession(kUser1Email, /*is_user_kiosk=*/true);

  // For non-regular user, the Bluetooth setting should not be applied and pref
  // not set.
  EXPECT_TRUE(active_user_prefs()
                  ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
                  ->IsDefaultValue());
  EXPECT_FALSE(
      active_user_prefs()->GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabled);
}

// Tests how BluetoothPowerController applies the user pref when
// the pref has been set before.
TEST_F(BluetoothPowerControllerImplTest, ApplyBluetoothPrimaryUserPrefOn) {
  // Set the pref to true.
  active_user_prefs()->SetBoolean(prefs::kUserBluetoothAdapterEnabled, true);

  // Start with Bluetooth power off.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);

  Init();

  EXPECT_FALSE(active_user_prefs()
                   ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
                   ->IsDefaultValue());
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabled);

  AddUserSession(kUser1Email);

  // Pref should be applied to trigger the Bluetooth power on, and the pref
  // value should be unchanged.
  EXPECT_TRUE(
      active_user_prefs()->GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabling);
}

TEST_F(BluetoothPowerControllerImplTest,
       EnableBluetoothWithoutPersistence_LocalStatePrefOn) {
  Init();

  // Pref should be set to enabled.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabled);

  // Set Bluetooth enabled.
  SetBluetoothEnabledWithoutPersistence();

  // The pref and adapter state should remain unchanged.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabled);

  // Set HID detection inactive with no device using Bluetooth.
  SetBluetoothHidDetectionInactive(/*is_using_bluetooth=*/false);

  // The pref and adapter state should remain unchanged.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabled);
}

TEST_F(BluetoothPowerControllerImplTest,
       EnableBluetoothWithoutPersistence_LocalStatePrefOff_BluetoothUnused) {
  Init();

  // Turn Bluetooth off.
  SetBluetoothEnabledState(false);

  // The adapter should disable and the pref set to disabled.
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabling);
  EXPECT_FALSE(
      local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  // Set Bluetooth enabled.
  SetBluetoothEnabledWithoutPersistence();

  // The adapter should enable but the pref remain unchanged.
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabling);
  EXPECT_FALSE(
      local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  // Set HID detection inactive.
  SetBluetoothHidDetectionInactive(/*is_using_bluetooth=*/false);

  // The adapter should disable and the pref remain unchanged.
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabling);
  EXPECT_FALSE(
      local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
}

TEST_F(BluetoothPowerControllerImplTest,
       EnableBluetoothWithoutPersistence_LocalStatePrefOff_BluetoothUsed) {
  Init();

  // Turn Bluetooth off.
  SetBluetoothEnabledState(false);

  // The adapter should disable and the pref set to disabled.
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kDisabling);
  EXPECT_FALSE(
      local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  // Set Bluetooth enabled.
  SetBluetoothEnabledWithoutPersistence();

  // The adapter should enable but the pref remain unchanged.
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabling);
  EXPECT_FALSE(
      local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  // Set HID detection inactive with Bluetooth being used.
  SetBluetoothHidDetectionInactive(/*is_using_bluetooth=*/true);

  // The adapter state should remain unchanged and the pref set to enabled.
  EXPECT_EQ(GetAdapterState(), mojom::BluetoothSystemState::kEnabling);
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
}

}  // namespace ash::bluetooth_config
