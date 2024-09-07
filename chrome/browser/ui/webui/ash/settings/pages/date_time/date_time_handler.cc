// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/date_time/date_time_handler.h"

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/login_screen.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/set_time/set_time_dialog.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace ash::settings {

namespace {

// Returns whether the system's automatic time zone detection setting is
// managed, which may override the user's setting.
bool IsSystemTimezoneAutomaticDetectionManaged() {
  return g_browser_process->local_state()->IsManagedPreference(
      prefs::kSystemTimezoneAutomaticDetectionPolicy);
}

// Returns the system's automatic time zone detection policy value, which
// corresponds to the SystemTimezoneProto's AutomaticTimezoneDetectionType
// enum and determines whether the user's setting will be overridden.
int GetSystemTimezoneAutomaticDetectionPolicyValue() {
  DCHECK(IsSystemTimezoneAutomaticDetectionManaged());

  return g_browser_process->local_state()->GetInteger(
      prefs::kSystemTimezoneAutomaticDetectionPolicy);
}

// Returns whether the user can set the automatic detection setting, based on
// flags and policies.
bool IsTimezoneAutomaticDetectionUserEditable() {
  if (system::HasSystemTimezonePolicy()) {
    return false;
  }

  if (IsSystemTimezoneAutomaticDetectionManaged()) {
    return GetSystemTimezoneAutomaticDetectionPolicyValue() ==
           enterprise_management::SystemTimezoneProto::USERS_DECIDE;
  }

  return true;
}

}  // namespace

DateTimeHandler::DateTimeHandler() {}

DateTimeHandler::~DateTimeHandler() = default;

void DateTimeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "dateTimePageReady",
      base::BindRepeating(&DateTimeHandler::HandleDateTimePageReady,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTimeZones", base::BindRepeating(&DateTimeHandler::HandleGetTimeZones,
                                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showSetDateTimeUI",
      base::BindRepeating(&DateTimeHandler::HandleShowSetDateTimeUI,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "handleShowParentAccessForTimeZone",
      base::BindRepeating(&DateTimeHandler::HandleShowParentAccessForTimeZone,
                          base::Unretained(this)));
}

void DateTimeHandler::OnJavascriptAllowed() {
  SystemClockClient* system_clock_client = SystemClockClient::Get();
  scoped_observation_.Observe(system_clock_client);
  SystemClockCanSetTimeChanged(system_clock_client->CanSetTime());

  // The system time zone policy disables auto-detection entirely. (However,
  // the time zone policy does not override the user's time zone itself.)
  system_timezone_policy_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kSystemTimezonePolicy,
          base::BindRepeating(
              &DateTimeHandler::NotifyTimezoneAutomaticDetectionPolicy,
              weak_ptr_factory_.GetWeakPtr()));

  // The auto-detection policy can force auto-detection on or off.
  local_state_pref_change_registrar_.Init(g_browser_process->local_state());
  local_state_pref_change_registrar_.Add(
      prefs::kSystemTimezoneAutomaticDetectionPolicy,
      base::BindRepeating(
          &DateTimeHandler::NotifyTimezoneAutomaticDetectionPolicy,
          base::Unretained(this)));
}

void DateTimeHandler::OnJavascriptDisallowed() {
  scoped_observation_.Reset();
  system_timezone_policy_subscription_ = {};
  local_state_pref_change_registrar_.RemoveAll();
}

void DateTimeHandler::HandleDateTimePageReady(const base::Value::List& args) {
  AllowJavascript();

  // Send the time zone automatic detection policy in case it changed after the
  // handler was created.
  NotifyTimezoneAutomaticDetectionPolicy();
}

void DateTimeHandler::HandleGetTimeZones(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, system::GetTimezoneList());
}

void DateTimeHandler::HandleShowSetDateTimeUI(const base::Value::List& args) {
  // Make sure the clock status hasn't changed since the button was clicked.
  if (!SystemClockClient::Get()->CanSetTime()) {
    return;
  }
  SetTimeDialog::ShowDialog(
      web_ui()->GetWebContents()->GetTopLevelNativeWindow());
}

void DateTimeHandler::HandleShowParentAccessForTimeZone(
    const base::Value::List& args) {
  DCHECK(user_manager::UserManager::Get()->GetActiveUser()->IsChild());

  if (!parent_access::ParentAccessService::IsApprovalRequired(
          SupervisedAction::kUpdateTimezone)) {
    OnParentAccessValidation(true);
    return;
  }

  ParentAccessController::Get()->ShowWidget(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
      base::BindOnce(&DateTimeHandler::OnParentAccessValidation,
                     weak_ptr_factory_.GetWeakPtr()),
      SupervisedAction::kUpdateTimezone, false /* extra_dimmer */,
      base::Time::Now());
}

void DateTimeHandler::OnParentAccessValidation(bool success) {
  if (success) {
    FireWebUIListener("access-code-validation-complete");
  }
}

void DateTimeHandler::NotifyTimezoneAutomaticDetectionPolicy() {
  bool managed = !IsTimezoneAutomaticDetectionUserEditable();
  bool force_enabled = managed && g_browser_process->platform_part()
                                      ->GetTimezoneResolverManager()
                                      ->ShouldApplyResolvedTimezone();

  FireWebUIListener("time-zone-auto-detect-policy", base::Value(managed),
                    base::Value(force_enabled));
}

void DateTimeHandler::SystemClockCanSetTimeChanged(bool can_set_time) {
  FireWebUIListener("can-set-date-time-changed", base::Value(can_set_time));
}

}  // namespace ash::settings
