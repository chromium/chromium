// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/peripheral_data_access_handler.h"

#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace ash::settings {

namespace {
constexpr char thunderbolt_file_path[] = "/sys/bus/thunderbolt/devices/0-0";
constexpr char local_state_pref_name[] =
    "settings.local_state_device_pci_data_access_enabled";
constexpr char cros_setting_pref_name[] =
    "cros.device.peripheral_data_access_enabled";
}  // namespace

bool CheckIfThunderboltFilepathExists() {
  return base::PathExists(base::FilePath(thunderbolt_file_path));
}

// static
bool PeripheralDataAccessHandler::GetPrefState() {
  // If the device is managed, use the local state pref.
  if (InstallAttributes::Get()->IsEnterpriseManaged()) {
    return g_browser_process->local_state()->GetBoolean(
        ash::prefs::kLocalStateDevicePeripheralDataAccessEnabled);
  }

  // Otherwise, use the CrosSetting for non-managed devices.
  bool pcie_tunneling_allowed = false;
  CrosSettings::Get()->GetBoolean(kDevicePeripheralDataAccessEnabled,
                                  &pcie_tunneling_allowed);
  return pcie_tunneling_allowed;
}

PeripheralDataAccessHandler::PeripheralDataAccessHandler() {
  auto* pref = g_browser_process->local_state()->FindPreference(
      ash::prefs::kLocalStateDevicePeripheralDataAccessEnabled);
  DCHECK(pref);
  // If the user has a managed policy or is a guest profile, prevent user
  // configuration of the setting.
  is_user_configurable_ = !pref->IsManaged() && !IsGuestModeActive();

  peripheral_data_access_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDevicePeripheralDataAccessEnabled,
          base::BindRepeating(&PeripheralDataAccessHandler::
                                  OnPeripheralDataAccessProtectionChanged,
                              base::Unretained(this)));
}

PeripheralDataAccessHandler::~PeripheralDataAccessHandler() = default;

void PeripheralDataAccessHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "isThunderboltSupported",
      base::BindRepeating(
          &PeripheralDataAccessHandler::HandleThunderboltSupported,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getPolicyState",
      base::BindRepeating(&PeripheralDataAccessHandler::HandleGetPolicyState,
                          base::Unretained(this)));
}

void PeripheralDataAccessHandler::OnJavascriptAllowed() {}

void PeripheralDataAccessHandler::OnJavascriptDisallowed() {}

void PeripheralDataAccessHandler::HandleThunderboltSupported(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1u, args.size());
  const std::string& callback_id = args[0].GetString();

  // PathExist is a blocking call. PostTask it and wait on the result.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CheckIfThunderboltFilepathExists),
      base::BindOnce(&PeripheralDataAccessHandler::OnFilePathChecked,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void PeripheralDataAccessHandler::HandleGetPolicyState(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1u, args.size());
  const std::string& callback_id = args[0].GetString();

  const std::string& pref_name = InstallAttributes::Get()->IsEnterpriseManaged()
                                     ? local_state_pref_name
                                     : cros_setting_pref_name;

  base::Value::Dict response;
  response.Set("prefName", pref_name);
  response.Set("isUserConfigurable", is_user_configurable_);
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void PeripheralDataAccessHandler::OnFilePathChecked(
    const std::string& callback_id, bool is_thunderbolt_supported) {
  ResolveJavascriptCallback(base::Value(callback_id),
      base::Value(is_thunderbolt_supported));
}

void PeripheralDataAccessHandler::OnPeripheralDataAccessProtectionChanged() {
  DCHECK(PciguardClient::Get());

  bool new_state = false;
  CrosSettings::Get()->GetBoolean(kDevicePeripheralDataAccessEnabled,
                                  &new_state);

  PeripheralNotificationManager::Get()->SetPcieTunnelingAllowedState(new_state);
  PciguardClient::Get()->SendExternalPciDevicesPermissionState(new_state);
  TypecdClient::Get()->SetPeripheralDataAccessPermissionState(new_state);
}

}  // namespace ash::settings
