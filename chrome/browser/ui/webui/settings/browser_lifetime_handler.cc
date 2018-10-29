// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/lifetime/application_lifetime.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

namespace settings {

namespace {

#if defined(OS_CHROMEOS)
// Triggers a TPM firmware update using the least destructive mode from
// |available_modes|.
void TriggerTPMFirmwareUpdate(
    const std::set<chromeos::tpm_firmware_update::Mode>& available_modes) {
  using chromeos::tpm_firmware_update::Mode;

  // Decide which update mode to use.
  for (Mode mode :
       {Mode::kPreserveDeviceState, Mode::kPowerwash, Mode::kCleanup}) {
    if (available_modes.count(mode) == 0) {
      continue;
    }

    // Save a TPM firmware update request in local state, which
    // will trigger the reset screen to appear on reboot.
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kFactoryResetRequested, true);
    prefs->SetInteger(prefs::kFactoryResetTPMFirmwareUpdateMode,
                      static_cast<int>(mode));
    prefs->CommitPendingWrite();
    chrome::AttemptRelaunch();
    return;
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

BrowserLifetimeHandler::BrowserLifetimeHandler() {}

BrowserLifetimeHandler::~BrowserLifetimeHandler() {}

void BrowserLifetimeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "restart", base::BindRepeating(&BrowserLifetimeHandler::HandleRestart,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "relaunch", base::BindRepeating(&BrowserLifetimeHandler::HandleRelaunch,
                                      base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "signOutAndRestart",
      base::BindRepeating(&BrowserLifetimeHandler::HandleSignOutAndRestart,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "factoryReset",
      base::BindRepeating(&BrowserLifetimeHandler::HandleFactoryReset,
                          base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)
}

void BrowserLifetimeHandler::HandleRestart(
    const base::ListValue* args) {
  chrome::AttemptRestart();
}

void BrowserLifetimeHandler::HandleRelaunch(
    const base::ListValue* args) {
  chrome::AttemptRelaunch();
}

#if defined(OS_CHROMEOS)
void BrowserLifetimeHandler::HandleSignOutAndRestart(
    const base::ListValue* args) {
  chrome::AttemptUserExit();
}

void BrowserLifetimeHandler::HandleFactoryReset(
    const base::ListValue* args) {
  const base::Value::ListStorage& args_list = args->GetList();
  CHECK_EQ(1U, args_list.size());
  bool tpm_firmware_update_requested = args_list[0].GetBool();

  if (tpm_firmware_update_requested) {
    chromeos::tpm_firmware_update::GetAvailableUpdateModes(
        base::BindOnce(&TriggerTPMFirmwareUpdate), base::TimeDelta());
    return;
  }

  // TODO(crbug.com/891905): Centralize powerwash restriction checks.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool allow_powerwash =
      !connector->IsEnterpriseManaged() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
      !user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser() &&
      !user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  if (!allow_powerwash)
    return;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  // Perform sign out. Current chrome process will then terminate, new one will
  // be launched (as if it was a restart).
  chrome::AttemptRelaunch();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
