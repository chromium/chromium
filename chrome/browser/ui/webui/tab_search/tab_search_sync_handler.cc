// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_sync_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/core_account_id.h"
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
  syncer::SyncService* sync_service = GetSyncService();
  if (sync_service && !sync_service_observation_.IsObserving()) {
    sync_service_observation_.Observe(sync_service);
  }

  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager && !identity_manager_observation_.IsObserving()) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

void TabSearchSyncHandler::OnJavascriptDisallowed() {
  sync_service_observation_.Reset();
  identity_manager_observation_.Reset();
}

bool TabSearchSyncHandler::GetSignInState() const {
  const signin::IdentityManager* const identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  const bool signed_out = primary_account_id.empty();
  const bool has_error =
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id);
  return !signed_out && !has_error;
}

void TabSearchSyncHandler::HandleGetSignInState(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetSignInState());
}

void TabSearchSyncHandler::OnStateChanged(syncer::SyncService* sync_service) {
  FireWebUIListener("account-info-changed", GetSignInState());
}

void TabSearchSyncHandler::OnSyncShutdown(syncer::SyncService* sync_service) {
  sync_service_observation_.Reset();
}

void TabSearchSyncHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetSignInState());
}

void TabSearchSyncHandler::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetSignInState());
}

syncer::SyncService* TabSearchSyncHandler::GetSyncService() const {
  return SyncServiceFactory::IsSyncAllowed(profile_)
             ? SyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}
