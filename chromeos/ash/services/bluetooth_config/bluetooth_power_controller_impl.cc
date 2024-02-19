// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/bluetooth_power_controller_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::bluetooth_config {

namespace {

// Decides whether to apply Bluetooth setting based on user type.
// Returns true if the user type represents a human individual, currently this
// includes: regular, child, supervised, or active directory. The other types
// do not represent human account so those account should follow system-wide
// Bluetooth setting instead.
bool ShouldApplyUserBluetoothSetting(user_manager::UserType user_type) {
  return user_type == user_manager::UserType::kRegular ||
         user_type == user_manager::UserType::kChild;
}

}  // namespace

// static
void BluetoothPowerControllerImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kSystemBluetoothAdapterEnabled,
                                /*default_value=*/false);
}

// static
void BluetoothPowerControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUserBluetoothAdapterEnabled,
                                /*default_value=*/false);
}

BluetoothPowerControllerImpl::BluetoothPowerControllerImpl(
    AdapterStateController* adapter_state_controller)
    : adapter_state_controller_(adapter_state_controller) {
  adapter_state_controller_observation_.Observe(
      adapter_state_controller_.get());
}

BluetoothPowerControllerImpl::~BluetoothPowerControllerImpl() = default;

void BluetoothPowerControllerImpl::SetBluetoothEnabledState(bool enabled) {
  if (primary_profile_prefs_) {
    BLUETOOTH_LOG(EVENT) << "Saving Bluetooth power state of " << enabled
                         << " to user prefs.";

    primary_profile_prefs_->SetBoolean(prefs::kUserBluetoothAdapterEnabled,
                                       enabled);
  } else if (local_state_) {
    BLUETOOTH_LOG(EVENT) << "Saving Bluetooth power state of " << enabled
                         << " to local state.";

    local_state_->SetBoolean(prefs::kSystemBluetoothAdapterEnabled, enabled);
  } else {
    BLUETOOTH_LOG(ERROR)
        << "SetBluetoothEnabledState() called before preferences were set";
  }
  SetAdapterState(enabled);
}

void BluetoothPowerControllerImpl::SetBluetoothEnabledWithoutPersistence() {
  BLUETOOTH_LOG(EVENT) << "Enabling adapter without persistence...";
  SetAdapterState(true);
}

void BluetoothPowerControllerImpl::SetBluetoothHidDetectionInactive(
    bool is_using_bluetooth) {
  if (is_using_bluetooth) {
    BLUETOOTH_LOG(EVENT) << "HID detection finished and Bluetooth is being "
                         << "used. Ensuring Bluetooth is enabled and the "
                         << "enabled state persisted.";
    SetBluetoothEnabledState(true);
    return;
  }

  DCHECK(local_state_)
      << "HID detection finished but unable to restore persisted Bluetooth "
         "state because local_state_ is null.";
  DCHECK(!user_manager::UserManager::Get()->GetActiveUser());

  BLUETOOTH_LOG(EVENT)
      << "HID detection finished, restoring persisted Bluetooth state.";
  ApplyBluetoothLocalStatePref();
}

void BluetoothPowerControllerImpl::SetPrefs(PrefService* primary_profile_prefs,
                                            PrefService* local_state) {
  InitLocalStatePrefService(local_state);
  InitPrimaryUserPrefService(primary_profile_prefs);
}

void BluetoothPowerControllerImpl::OnAdapterStateChanged() {
  if (!pending_adapter_enabled_state_.has_value())
    return;

  if (adapter_state_controller_->GetAdapterState() ==
      mojom::BluetoothSystemState::kUnavailable) {
    return;
  }

  // Adapter is now available after being unavailable. Set adapter state to
  // |pending_adapter_enabled_state_|.
  bool enabled = pending_adapter_enabled_state_.value();
  BLUETOOTH_LOG(EVENT) << "Adapter is now available after being unavailable, "
                       << "setting adapter state to " << enabled;

  pending_adapter_enabled_state_.reset();

  SetAdapterState(enabled);
}

void BluetoothPowerControllerImpl::InitLocalStatePrefService(
    PrefService* local_state) {
  BLUETOOTH_LOG(EVENT) << "Initializing local state pref service";

  // Return early if |local_state_| has already been initialized or
  // |local_state| is invalid.
  if (local_state_) {
    BLUETOOTH_LOG(EVENT) << "Local state has already be initialized";
    return;
  }

  if (!local_state) {
    BLUETOOTH_LOG(EVENT) << "local_state is null, not initializing";
    return;
  }

  local_state_ = local_state;

  // Apply the local state pref if no user has logged in (still in login
  // screen).
  if (!user_manager::UserManager::Get()->GetActiveUser())
    ApplyBluetoothLocalStatePref();
}

void BluetoothPowerControllerImpl::ApplyBluetoothLocalStatePref() {
  if (local_state_->FindPreference(prefs::kSystemBluetoothAdapterEnabled)
          ->IsDefaultValue()) {
    // If the device has not had the local state Bluetooth pref set, this is a
    // fresh install. On fresh installs, the Bluetooth adapter defaults to
    // powered on. Save this state to prefs.
    BLUETOOTH_LOG(EVENT) << "No local state pref has been set, saving"
                         << "Bluetooth power state of enabled to local state";
    local_state_->SetBoolean(prefs::kSystemBluetoothAdapterEnabled, true);
    return;
  }

  bool enabled =
      local_state_->GetBoolean(prefs::kSystemBluetoothAdapterEnabled);
  BLUETOOTH_LOG(EVENT) << "Applying local state pref Bluetooth power: "
                       << enabled;
  SetAdapterState(enabled);
}

void BluetoothPowerControllerImpl::InitPrimaryUserPrefService(
    PrefService* primary_profile_prefs) {
  BLUETOOTH_LOG(EVENT) << "Initializing primary user pref service";

  primary_profile_prefs_ = primary_profile_prefs;
  if (!primary_profile_prefs_) {
    BLUETOOTH_LOG(EVENT) << "primary_profile_prefs_ is null, not initializing";
    return;
  }

  DCHECK_EQ(user_manager::UserManager::Get()->GetActiveUser(),
            user_manager::UserManager::Get()->GetPrimaryUser());

  if (!has_attempted_apply_primary_user_pref_) {
    BLUETOOTH_LOG(EVENT)
        << "Primary user pref has not been attempted to be applied, applying";
    ApplyBluetoothPrimaryUserPref();
    has_attempted_apply_primary_user_pref_ = true;
  }
}

void BluetoothPowerControllerImpl::ApplyBluetoothPrimaryUserPref() {
  std::optional<user_manager::UserType> user_type =
      user_manager::UserManager::Get()->GetActiveUser()->GetType();

  // Apply the Bluetooth pref only for regular users (i.e. users representing
  // a human individual). We don't want to apply Bluetooth pref for other users
  // e.g. kiosk, guest etc. For non-human users, Bluetooth power should be left
  // to the current power state.
  if (!user_type || !ShouldApplyUserBluetoothSetting(*user_type)) {
    BLUETOOTH_LOG(EVENT) << "Not applying primary user pref because user has "
                            "no type or is not a regular user.";
    return;
  }

  if (!primary_profile_prefs_
           ->FindPreference(prefs::kUserBluetoothAdapterEnabled)
           ->IsDefaultValue()) {
    bool enabled =
        primary_profile_prefs_->GetBoolean(prefs::kUserBluetoothAdapterEnabled);
    BLUETOOTH_LOG(EVENT) << "Applying primary user pref Bluetooth power: "
                         << enabled;
    SetAdapterState(enabled);
    return;
  }

  // If the user has not had the Bluetooth pref yet, set the user pref
  // according to whatever the current Bluetooth power is, except for
  // new users (first login on the device) always set the new pref to true.
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    BLUETOOTH_LOG(EVENT) << "Setting Bluetooth power to enabled for new user.";
    SetBluetoothEnabledState(true);
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Saving current power state of "
                       << adapter_state_controller_->GetAdapterState()
                       << " to user prefs.";
  SaveCurrentPowerStateToPrefs(primary_profile_prefs_,
                               prefs::kUserBluetoothAdapterEnabled);
}

void BluetoothPowerControllerImpl::SetAdapterState(bool enabled) {
  BLUETOOTH_LOG(EVENT) << "Setting adapter state to "
                       << (enabled ? "enabled " : "disabled");

  // On device startup, the local prefs may attempted to be applied before the
  // adapter is available. Cache the value so it can be set once the adapter
  // is available in OnAdapterStateChanged().
  if (adapter_state_controller_->GetAdapterState() ==
      mojom::BluetoothSystemState::kUnavailable) {
    BLUETOOTH_LOG(EVENT) << "Adapter is currently unavailable, setting "
                         << "pending_adapter_enabled_state_ to " << enabled;
    pending_adapter_enabled_state_ = enabled;
    return;
  }

  adapter_state_controller_->SetBluetoothEnabledState(enabled);
}

void BluetoothPowerControllerImpl::SaveCurrentPowerStateToPrefs(
    PrefService* prefs,
    const char* pref_name) {
  prefs->SetBoolean(pref_name,
                    IsBluetoothEnabledOrEnabling(
                        adapter_state_controller_->GetAdapterState()));
}

}  // namespace ash::bluetooth_config
