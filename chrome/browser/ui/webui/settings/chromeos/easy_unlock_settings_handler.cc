// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/easy_unlock_settings_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/proximity_auth/switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace settings {

EasyUnlockSettingsHandler::EasyUnlockSettingsHandler(Profile* profile)
    : profile_(profile) {
  profile_pref_registrar_.Init(profile->GetPrefs());
}

EasyUnlockSettingsHandler::~EasyUnlockSettingsHandler() {
  EasyUnlockService::Get(profile_)->RemoveObserver(this);
}

EasyUnlockSettingsHandler* EasyUnlockSettingsHandler::Create(
    content::WebUIDataSource* html_source,
    Profile* profile) {
  EasyUnlockService* easy_unlock_service = EasyUnlockService::Get(profile);
  // The service is not created for LockScreenApp profiles or "off the record".
  if (!easy_unlock_service)
    return nullptr;

  bool allowed = easy_unlock_service->IsAllowed();
  html_source->AddBoolean("easyUnlockAllowed", allowed);
  html_source->AddBoolean("easyUnlockEnabled",
                          allowed ? easy_unlock_service->IsEnabled() : false);
  // TODO(crbug.com/894585): Remove this legacy special case after M71.
  html_source->AddBoolean("easyUnlockInLegacyHostMode",
                          allowed && easy_unlock_service->IsInLegacyHostMode());

  if (!allowed)
    return nullptr;

  return new EasyUnlockSettingsHandler(profile);
}

void EasyUnlockSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "easyUnlockGetEnabledStatus",
      base::BindRepeating(&EasyUnlockSettingsHandler::HandleGetEnabledStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockStartTurnOnFlow",
      base::BindRepeating(&EasyUnlockSettingsHandler::HandleStartTurnOnFlow,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockGetTurnOffFlowStatus",
      base::BindRepeating(
          &EasyUnlockSettingsHandler::HandleGetTurnOffFlowStatus,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockStartTurnOffFlow",
      base::BindRepeating(&EasyUnlockSettingsHandler::HandleStartTurnOffFlow,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "easyUnlockCancelTurnOffFlow",
      base::BindRepeating(&EasyUnlockSettingsHandler::HandleCancelTurnOffFlow,
                          base::Unretained(this)));
}

void EasyUnlockSettingsHandler::OnJavascriptAllowed() {
  EasyUnlockService::Get(profile_)->AddObserver(this);

  profile_pref_registrar_.Add(
      prefs::kEasyUnlockPairing,
      base::Bind(&EasyUnlockSettingsHandler::SendEnabledStatus,
                 base::Unretained(this)));
}

void EasyUnlockSettingsHandler::OnJavascriptDisallowed() {
  EasyUnlockService::Get(profile_)->RemoveObserver(this);
  profile_pref_registrar_.RemoveAll();
}

void EasyUnlockSettingsHandler::OnTurnOffOperationStatusChanged() {
  FireWebUIListener("easy-unlock-turn-off-flow-status",
                    base::Value(GetTurnOffFlowStatus()));
}

void EasyUnlockSettingsHandler::SendEnabledStatus() {
  CallJavascriptFunction(
      "cr.webUIListenerCallback", base::Value("easy-unlock-enabled-status"),
      base::Value(EasyUnlockService::Get(profile_)->IsEnabled()));
}

std::string EasyUnlockSettingsHandler::GetTurnOffFlowStatus() {
  EasyUnlockService::TurnOffFlowStatus status =
      EasyUnlockService::Get(profile_)->GetTurnOffFlowStatus();

  // Translate status into JS UI state string. Note the translated string
  // should match UIState defined in easy_unlock_turn_off_dialog.js.
  std::string status_string;
  switch (status) {
    case EasyUnlockService::IDLE:
      status_string = "idle";
      break;
    case EasyUnlockService::PENDING:
      status_string = "pending";
      break;
    case EasyUnlockService::FAIL:
      status_string = "server-error";
      break;
    default:
      LOG(ERROR) << "Unknown Easy unlock turn-off operation status: " << status;
      status_string = "idle";
      break;
  }

  return status_string;
}

void EasyUnlockSettingsHandler::HandleGetEnabledStatus(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(
      *callback_id, base::Value(EasyUnlockService::Get(profile_)->IsEnabled()));
}

void EasyUnlockSettingsHandler::HandleStartTurnOnFlow(
    const base::ListValue* args) {
  EasyUnlockService::Get(profile_)->LaunchSetup();
}

void EasyUnlockSettingsHandler::HandleGetTurnOffFlowStatus(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(*callback_id, base::Value(GetTurnOffFlowStatus()));
}

void EasyUnlockSettingsHandler::HandleStartTurnOffFlow(
    const base::ListValue* args) {
  EasyUnlockService::Get(profile_)->RunTurnOffFlow();
}

void EasyUnlockSettingsHandler::HandleCancelTurnOffFlow(
    const base::ListValue* args) {
  EasyUnlockService::Get(profile_)->ResetTurnOffFlow();
}

}  // namespace settings
}  // namespace chromeos
