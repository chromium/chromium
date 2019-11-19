// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_web_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

constexpr StaticOobeScreenId EnableDebuggingScreenView::kScreenId;

EnableDebuggingScreenHandler::EnableDebuggingScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {}

EnableDebuggingScreenHandler::~EnableDebuggingScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void EnableDebuggingScreenHandler::ShowWithParams() {
  ShowScreen(kScreenId);

  UpdateUIState(UI_STATE_WAIT);

  DVLOG(1) << "Showing enable debugging screen.";

  // Wait for cryptohomed before checking debugd. See http://crbug.com/440506.
  chromeos::CryptohomeClient* client = chromeos::CryptohomeClient::Get();
  client->WaitForServiceToBeAvailable(base::Bind(
      &EnableDebuggingScreenHandler::OnCryptohomeDaemonAvailabilityChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  ShowWithParams();
}

void EnableDebuggingScreenHandler::Hide() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EnableDebuggingScreenHandler::SetDelegate(EnableDebuggingScreen* screen) {
  screen_ = screen;
  if (page_is_ready())
    Initialize();
}

void EnableDebuggingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("enableDebuggingScreenTitle",
               IDS_ENABLE_DEBUGGING_SCREEN_TITLE);
  builder->Add("enableDebuggingScreenAccessibleTitle",
               IDS_ENABLE_DEBUGGING_SCREEN_TITLE);
  builder->Add("enableDebuggingCancelButton", IDS_CANCEL);
  builder->Add("enableDebuggingOKButton", IDS_OK);
  builder->Add("enableDebuggingRemoveButton",
               IDS_ENABLE_DEBUGGING_REMOVE_ROOTFS_BUTTON);
  builder->Add("enableDebuggingEnableButton",
               IDS_ENABLE_DEBUGGING_ENABLE_BUTTON);
  builder->Add("enableDebuggingRemveRootfsMessage",
               IDS_ENABLE_DEBUGGING_SCREEN_ROOTFS_REMOVE_MSG);
  builder->Add("enableDebuggingLearnMore",
               IDS_ENABLE_DEBUGGING_LEARN_MORE);
  builder->Add("enableDebuggingSetupMessage",
               IDS_ENABLE_DEBUGGING_SETUP_MESSAGE);
  builder->Add("enableDebuggingWaitMessage",
               IDS_ENABLE_DEBUGGING_WAIT_MESSAGE);
  builder->AddF("enableDebuggingWarningTitle",
                IDS_ENABLE_DEBUGGING_SCREEN_WARNING_MSG,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("enableDebuggingDoneMessage",
                IDS_ENABLE_DEBUGGING_DONE_MESSAGE,
                IDS_SHORT_PRODUCT_NAME);
  builder->Add("enableDebuggingErrorTitle",
                IDS_ENABLE_DEBUGGING_ERROR_TITLE);
  builder->AddF("enableDebuggingErrorMessage",
                IDS_ENABLE_DEBUGGING_ERROR_MESSAGE,
                IDS_SHORT_PRODUCT_NAME);
  builder->Add("enableDebuggingPasswordLabel",
               IDS_ENABLE_DEBUGGING_ROOT_PASSWORD_LABEL);
  builder->Add("enableDebuggingConfirmPasswordLabel",
               IDS_ENABLE_DEBUGGING_CONFIRM_PASSWORD_LABEL);
  builder->Add("enableDebuggingPasswordLengthNote",
               IDS_ENABLE_DEBUGGING_EMPTY_ROOT_PASSWORD_LABEL);
}

// static
void EnableDebuggingScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDebuggingFeaturesRequested, false);
}

void EnableDebuggingScreenHandler::Initialize() {
  if (!page_is_ready() || !screen_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EnableDebuggingScreenHandler::RegisterMessages() {
  AddCallback("enableDebuggingOnCancel",
              &EnableDebuggingScreenHandler::HandleOnCancel);
  AddCallback("enableDebuggingOnDone",
              &EnableDebuggingScreenHandler::HandleOnDone);
  AddCallback("enableDebuggingOnLearnMore",
              &EnableDebuggingScreenHandler::HandleOnLearnMore);
  AddCallback("enableDebuggingOnRemoveRootFSProtection",
              &EnableDebuggingScreenHandler::HandleOnRemoveRootFSProtection);
  AddCallback("enableDebuggingOnSetup",
              &EnableDebuggingScreenHandler::HandleOnSetup);
}

void EnableDebuggingScreenHandler::HandleOnCancel() {
  if (screen_)
    screen_->OnExit(false);
}

void EnableDebuggingScreenHandler::HandleOnDone() {
  if (screen_)
    screen_->OnExit(true);
}

void EnableDebuggingScreenHandler::HandleOnRemoveRootFSProtection() {
  UpdateUIState(UI_STATE_WAIT);
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->RemoveRootfsVerification(
      base::Bind(&EnableDebuggingScreenHandler::OnRemoveRootfsVerification,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreenHandler::HandleOnSetup(
    const std::string& password) {
  UpdateUIState(UI_STATE_WAIT);
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->EnableDebuggingFeatures(
      password,
      base::Bind(&EnableDebuggingScreenHandler::OnEnableDebuggingFeatures,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreenHandler::OnCryptohomeDaemonAvailabilityChecked(
    bool service_is_available) {
  DVLOG(1) << "Enable-debugging-screen: cryptohomed availability="
           << service_is_available;
  if (!service_is_available) {
    LOG(ERROR) << "Crypthomed is not available.";
    UpdateUIState(UI_STATE_ERROR);
    return;
  }

  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->WaitForServiceToBeAvailable(base::Bind(
      &EnableDebuggingScreenHandler::OnDebugDaemonServiceAvailabilityChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void EnableDebuggingScreenHandler::OnDebugDaemonServiceAvailabilityChecked(
    bool service_is_available) {
  DVLOG(1) << "Enable-debugging-screen: debugd availability="
           << service_is_available;
  if (!service_is_available) {
    LOG(ERROR) << "Debug daemon is not available.";
    UpdateUIState(UI_STATE_ERROR);
    return;
  }

  // Check the status of debugging features.
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->QueryDebuggingFeatures(
      base::Bind(&EnableDebuggingScreenHandler::OnQueryDebuggingFeatures,
                 weak_ptr_factory_.GetWeakPtr()));
}

// Removes rootfs verification, add flag to start with enable debugging features
// screen and reboots the machine.
void EnableDebuggingScreenHandler::OnRemoveRootfsVerification(bool success) {
  if (!success) {
    UpdateUIState(UI_STATE_ERROR);
    return;
  }

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDebuggingFeaturesRequested, true);
  prefs->CommitPendingWrite();
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER,
      "login debugging screen removing rootfs verification");
}

void EnableDebuggingScreenHandler::OnEnableDebuggingFeatures(bool success) {
  if (!success) {
    UpdateUIState(UI_STATE_ERROR);
    return;
  }

  UpdateUIState(UI_STATE_DONE);
}

void EnableDebuggingScreenHandler::OnQueryDebuggingFeatures(bool success,
                                                            int features_flag) {
  DVLOG(1) << "Enable-debugging-screen: OnQueryDebuggingFeatures"
           << ", success=" << success
           << ", features=" << features_flag;
  if (!success ||
      features_flag == debugd::DevFeatureFlag::DEV_FEATURES_DISABLED) {
    UpdateUIState(UI_STATE_ERROR);
    return;
  }

  if ((features_flag &
       debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED) == 0) {
    UpdateUIState(UI_STATE_REMOVE_PROTECTION);
    return;
  }

  if ((features_flag & DebugDaemonClient::DEV_FEATURE_ALL_ENABLED) !=
      DebugDaemonClient::DEV_FEATURE_ALL_ENABLED) {
    UpdateUIState(UI_STATE_SETUP);
  } else {
    UpdateUIState(UI_STATE_DONE);
  }
}

void EnableDebuggingScreenHandler::UpdateUIState(
    EnableDebuggingScreenHandler::UIState state) {
  if (state == UI_STATE_SETUP ||
      state == UI_STATE_ERROR ||
      state == UI_STATE_DONE) {
    PrefService* prefs = g_browser_process->local_state();
    prefs->ClearPref(prefs::kDebuggingFeaturesRequested);
    prefs->CommitPendingWrite();
  }

  CallJS("login.EnableDebuggingScreen.updateState", static_cast<int>(state));
}

void EnableDebuggingScreenHandler::HandleOnLearnMore() {
  VLOG(1) << "Trying to view the help article about debugging features.";
  const std::string help_content =
      l10n_util::GetStringUTF8(IDS_ENABLE_DEBUGGING_HELP);
  const GURL data_url = GURL("data:text/html;charset=utf-8," + help_content);

  LoginWebDialog* dialog =
      new LoginWebDialog(Profile::FromWebUI(web_ui()), NULL,
                         LoginDisplayHost::default_host()->GetNativeWindow(),
                         base::string16(), data_url);
  dialog->Show();
}

}  // namespace chromeos
