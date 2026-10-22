// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/quick_unlock_handler.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"

namespace ash::settings {

QuickUnlockHandler::QuickUnlockHandler(Profile* profile,
                                       PrefService* pref_service)
    : profile_(profile), pref_service_(pref_service) {}

QuickUnlockHandler::~QuickUnlockHandler() = default;

void QuickUnlockHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "RequestPinLoginState",
      base::BindRepeating(&QuickUnlockHandler::HandleRequestPinLoginState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "RequestActiveAuthFactors",
      base::BindRepeating(&QuickUnlockHandler::HandleRequestActiveAuthFactors,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "RequestQuickUnlockDisabledByPolicy",
      base::BindRepeating(
          &QuickUnlockHandler::HandleQuickUnlockDisabledByPolicy,
          base::Unretained(this)));
}

void QuickUnlockHandler::OnJavascriptAllowed() {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kQuickUnlockModeAllowlist,
      base::BindRepeating(
          &QuickUnlockHandler::UpdateQuickUnlockDisabledByPolicy,
          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kWebAuthnFactors,
      base::BindRepeating(
          &QuickUnlockHandler::UpdateQuickUnlockDisabledByPolicy,
          weak_ptr_factory_.GetWeakPtr()));
}

void QuickUnlockHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();
}

void QuickUnlockHandler::HandleRequestPinLoginState(
    const base::ListValue& args) {
  AllowJavascript();
  quick_unlock::PinBackend::GetInstance()->HasLoginSupport(
      base::BindOnce(&QuickUnlockHandler::OnPinLoginAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void QuickUnlockHandler::HandleQuickUnlockDisabledByPolicy(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());

  UpdateQuickUnlockDisabledByPolicy();
}

void QuickUnlockHandler::HandleRequestActiveAuthFactors(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);

  if (!user) {
    base::DictValue factors;
    factors.Set("password", false);
    factors.Set("pin", false);
    ResolveJavascriptCallback(callback_id, std::move(factors));
    return;
  }

  user_data_auth::ListAuthFactorsRequest request;
  request.mutable_account_id()->set_account_id(
      cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId())
          .account_id());

  UserDataAuthClient::Get()->ListAuthFactors(
      request,
      base::BindOnce(&QuickUnlockHandler::OnGetAuthFactors,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.GetString()));
}

void QuickUnlockHandler::OnGetAuthFactors(
    std::string callback_id,
    std::optional<user_data_auth::ListAuthFactorsReply> reply) {
  bool has_password = false;
  bool has_pin = false;
  if (reply.has_value()) {
    for (const auto& factor : reply->configured_auth_factors()) {
      switch (factor.type()) {
        case user_data_auth::AUTH_FACTOR_TYPE_PASSWORD:
          has_password = true;
          break;
        case user_data_auth::AUTH_FACTOR_TYPE_PIN:
          has_pin = true;
          break;
        default:
          break;
      }
    }
  }

  base::DictValue factors;
  factors.Set("password", has_password);
  factors.Set("pin", has_pin);
  ResolveJavascriptCallback(base::Value(callback_id), std::move(factors));
}

void QuickUnlockHandler::OnPinLoginAvailable(bool is_available) {
  FireWebUIListener("pin-login-available-changed", base::Value(is_available));
}

void QuickUnlockHandler::UpdateQuickUnlockDisabledByPolicy() {
  FireWebUIListener("quick-unlock-disabled-by-policy-changed",
                    base::Value(quick_unlock::IsPinDisabledByPolicy(
                        pref_service_, quick_unlock::Purpose::kAny)));
}

}  // namespace ash::settings
