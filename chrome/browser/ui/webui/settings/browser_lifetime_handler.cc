// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/grit/branded_strings.h"
#include "components/policy/core/common/management/management_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/browser_list.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace settings {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Triggers a TPM firmware update using the least destructive mode from
// |available_modes|.
void TriggerTPMFirmwareUpdate(
    const std::set<ash::tpm_firmware_update::Mode>& available_modes) {
  using ::ash::tpm_firmware_update::Mode;

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "signOutAndRestart",
      base::BindRepeating(&BrowserLifetimeHandler::HandleSignOutAndRestart,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "factoryReset",
      base::BindRepeating(&BrowserLifetimeHandler::HandleFactoryReset,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "shouldShowRelaunchConfirmationDialog",
      base::BindRepeating(
          &BrowserLifetimeHandler::HandleShouldShowRelaunchConfirmationDialog,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getRelaunchConfirmationDialogDescription",
      base::BindRepeating(&BrowserLifetimeHandler::
                              HandleGetRelaunchConfirmationDialogDescription,
                          base::Unretained(this)));
#endif
}

void BrowserLifetimeHandler::HandleRestart(const base::Value::List& args) {
  chrome::AttemptRestart();
}

void BrowserLifetimeHandler::HandleRelaunch(const base::Value::List& args) {
  chrome::AttemptRelaunch();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void BrowserLifetimeHandler::HandleSignOutAndRestart(
    const base::Value::List& args) {
  chrome::AttemptUserExit();
}

void BrowserLifetimeHandler::HandleFactoryReset(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  bool tpm_firmware_update_requested = args[0].GetBool();

  if (tpm_firmware_update_requested) {
    ash::tpm_firmware_update::GetAvailableUpdateModes(
        base::BindOnce(&TriggerTPMFirmwareUpdate), base::TimeDelta());
    return;
  }

  // TODO(crbug.com/40596547): Centralize powerwash restriction checks.
  bool allow_powerwash =
      !policy::ManagementServiceFactory::GetForPlatform()->IsManaged() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void BrowserLifetimeHandler::HandleGetRelaunchConfirmationDialogDescription(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  CHECK(args[1].is_bool());
  const bool is_version_update = args[1].GetBool();

  size_t incognito_count = BrowserList::GetIncognitoBrowserCount();
  base::Value description;

  // The caller can specify if this is a confirmation dialog for browser version
  // update relaunch.
  if (is_version_update) {
    // The dialog description informs about a browser update after relaunch and
    // warns about incognito windows closure if any is open.
    description = base::Value(l10n_util::GetPluralStringFUTF16(
        IDS_UPDATE_RECOMMENDED, incognito_count));
  } else if (incognito_count > 0) {
    // The dialog description warns about incognito windows being closed after
    // relaunch.
    description = base::Value(l10n_util::GetPluralStringFUTF16(
        IDS_RELAUNCH_CONFIRMATION_DIALOG_BODY, incognito_count));
  }

  ResolveJavascriptCallback(callback_id, description);
}

void BrowserLifetimeHandler::HandleShouldShowRelaunchConfirmationDialog(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  CHECK(args[1].is_bool());
  const bool alwaysShowDialog = args[1].GetBool();

  // The caller can specify if the dialog should always be shown for a given
  // case by passing alwaysShowDialog parameter.
  if (alwaysShowDialog) {
    // Always show a confirmation dialog before the restart.
    ResolveJavascriptCallback(callback_id, true);
  } else {
    // Show a confirmation dialog before the restart if there is an incognito
    // window open.
    base::Value result =
        base::Value(BrowserList::GetIncognitoBrowserCount() > 0);
    ResolveJavascriptCallback(callback_id, result);
  }
}
#endif

}  // namespace settings
