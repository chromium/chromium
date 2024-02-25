// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/quick_unlock_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
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
    const base::Value::List& args) {
  AllowJavascript();
  quick_unlock::PinBackend::GetInstance()->HasLoginSupport(
      base::BindOnce(&QuickUnlockHandler::OnPinLoginAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void QuickUnlockHandler::HandleQuickUnlockDisabledByPolicy(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());

  UpdateQuickUnlockDisabledByPolicy();
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
