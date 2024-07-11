// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_sync_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"

TabSearchSyncHandler::TabSearchSyncHandler(Profile* profile)
    : profile_(profile) {}

TabSearchSyncHandler::~TabSearchSyncHandler() = default;

void TabSearchSyncHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "GetSignInState",
      base::BindRepeating(&TabSearchSyncHandler::HandleGetSignInState,
                          base::Unretained(this)));
}

void TabSearchSyncHandler::OnJavascriptAllowed() {
  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager && !identity_manager_observation_.IsObserving()) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

void TabSearchSyncHandler::OnJavascriptDisallowed() {
  identity_manager_observation_.Reset();
}

bool TabSearchSyncHandler::GetSignInState() const {
  const signin::IdentityManager* const identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  const auto stored_account = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  return stored_account.IsValid();
}

void TabSearchSyncHandler::HandleGetSignInState(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetSignInState());
}

void TabSearchSyncHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetSignInState());
}

void TabSearchSyncHandler::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetSignInState());
}
