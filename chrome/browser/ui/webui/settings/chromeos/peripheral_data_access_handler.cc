// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/peripheral_data_access_handler.h"

#include <string>
#include <utility>

#include "ash/components/pcie_peripheral/pcie_peripheral_manager.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/pciguard/pciguard_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace settings {

namespace {
constexpr char thunderbolt_file_path[] = "/sys/bus/thunderbolt/devices/0-0";
}  // namespace

bool CheckIfThunderboltFilepathExists() {
  return base::PathExists(base::FilePath(thunderbolt_file_path));
}

PeripheralDataAccessHandler::PeripheralDataAccessHandler() {
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
}

void PeripheralDataAccessHandler::OnJavascriptAllowed() {}

void PeripheralDataAccessHandler::OnJavascriptDisallowed() {}

void PeripheralDataAccessHandler::HandleThunderboltSupported(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1u, args->GetSize());
  const std::string& callback_id = args->GetList()[0].GetString();

  // PathExist is a blocking call. PostTask it and wait on the result.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CheckIfThunderboltFilepathExists),
      base::BindOnce(&PeripheralDataAccessHandler::OnFilePathChecked,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void PeripheralDataAccessHandler::OnFilePathChecked(
    const std::string& callback_id, bool is_thunderbolt_supported) {
  ResolveJavascriptCallback(base::Value(callback_id),
      base::Value(is_thunderbolt_supported));
}

void PeripheralDataAccessHandler::OnPeripheralDataAccessProtectionChanged() {
  DCHECK(PciguardClient::Get());

  bool new_state = false;
  CrosSettings::Get()->GetBoolean(chromeos::kDevicePeripheralDataAccessEnabled,
                                  &new_state);

  ash::PciePeripheralManager::Get()->SetPcieTunnelingAllowedState(new_state);
  PciguardClient::Get()->SendExternalPciDevicesPermissionState(new_state);
}

}  // namespace settings
}  // namespace chromeos
