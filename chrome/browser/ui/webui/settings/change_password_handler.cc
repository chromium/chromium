// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/change_password_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/password_protection/metrics_util.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace settings {

using password_manager::metrics_util::PasswordType;
using safe_browsing::ChromePasswordProtectionService;
using safe_browsing::LoginReputationClientResponse;
using safe_browsing::RequestOutcome;

ChangePasswordHandler::ChangePasswordHandler(
    Profile* profile,
    safe_browsing::ChromePasswordProtectionService* service)
    : profile_(profile), service_(service) {
  DCHECK(service_);
}

ChangePasswordHandler::~ChangePasswordHandler() {}

void ChangePasswordHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeChangePasswordHandler",
      base::BindRepeating(&ChangePasswordHandler::HandleInitialize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "changePassword",
      base::BindRepeating(&ChangePasswordHandler::HandleChangePassword,
                          base::Unretained(this)));
}

void ChangePasswordHandler::OnJavascriptAllowed() {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kSafeBrowsingUnhandledGaiaPasswordReuses,
      base::Bind(&ChangePasswordHandler::UpdateChangePasswordCardVisibility,
                 base::Unretained(this)));
}

void ChangePasswordHandler::OnJavascriptDisallowed() {
  pref_registrar_.RemoveAll();
}

void ChangePasswordHandler::HandleInitialize(const base::ListValue* args) {
    AllowJavascript();
    UpdateChangePasswordCardVisibility();
}

void ChangePasswordHandler::HandleChangePassword(const base::ListValue* args) {
  service_->OnUserAction(
      web_ui()->GetWebContents(),
      service_->reused_password_account_type_for_last_shown_warning(),
      RequestOutcome::UNKNOWN,
      LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED, "unused_token",
      safe_browsing::WarningUIType::CHROME_SETTINGS,
      safe_browsing::WarningAction::CHANGE_PASSWORD);
}

void ChangePasswordHandler::UpdateChangePasswordCardVisibility() {
  FireWebUIListener(
      "change-password-visibility",
      base::Value(
          service_->IsWarningEnabled(
              service_
                  ->reused_password_account_type_for_last_shown_warning()) &&
          safe_browsing::ChromePasswordProtectionService::
              ShouldShowChangePasswordSettingUI(profile_)));
}

}  // namespace settings
