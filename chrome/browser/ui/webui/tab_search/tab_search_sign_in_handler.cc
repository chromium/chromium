// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_sign_in_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"

TabSearchSignInHandler::TabSearchSignInHandler(Profile* profile)
    : profile_(profile) {}

TabSearchSignInHandler::~TabSearchSignInHandler() = default;

void TabSearchSignInHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "GetSignInState",
      base::BindRepeating(&TabSearchSignInHandler::HandleGetSignInState,
                          base::Unretained(this)));
}

void TabSearchSignInHandler::OnJavascriptAllowed() {
  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager && !identity_manager_observation_.IsObserving()) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

void TabSearchSignInHandler::OnJavascriptDisallowed() {
  identity_manager_observation_.Reset();
}

bool TabSearchSignInHandler::GetSignInState() const {
  const signin::IdentityManager* const identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  const auto stored_account = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  return stored_account.IsValid();
}

void TabSearchSignInHandler::HandleGetSignInState(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetSignInState());
}

void TabSearchSignInHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetSignInState());
}

void TabSearchSignInHandler::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetSignInState());
}
