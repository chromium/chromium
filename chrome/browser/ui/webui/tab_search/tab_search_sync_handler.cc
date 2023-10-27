// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_sync_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"

TabSearchSyncHandler::TabSearchSyncHandler(Profile* profile)
    : profile_(profile) {}

TabSearchSyncHandler::~TabSearchSyncHandler() = default;

void TabSearchSyncHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "GetAccountInfo",
      base::BindRepeating(&TabSearchSyncHandler::HandleGetAccountInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "GetSyncInfo",
      base::BindRepeating(&TabSearchSyncHandler::HandleGetSyncInfo,
                          base::Unretained(this)));
}

void TabSearchSyncHandler::OnJavascriptAllowed() {
  syncer::SyncService* sync_service = GetSyncService();
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }

  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

void TabSearchSyncHandler::OnJavascriptDisallowed() {
  sync_service_observation_.Reset();
  identity_manager_observation_.Reset();
}

base::Value::Dict TabSearchSyncHandler::GetSyncInfo() const {
  base::Value::Dict dict;
  bool syncing = false;
  bool syncingHistory = false;
  bool paused = true;

  const syncer::SyncService* const sync_service = GetSyncService();
  // sync_service might be nullptr if SyncServiceFactory::IsSyncAllowed is
  // false.
  if (sync_service) {
    syncing = sync_service->IsSyncFeatureEnabled();
    paused = !sync_service->IsSyncFeatureActive();
    syncer::UserSelectableTypeSet types =
        sync_service->GetUserSettings()->GetSelectedTypes();
    syncingHistory = sync_service->IsSyncFeatureEnabled() &&
                     types.Has(syncer::UserSelectableType::kHistory);
  }

  dict.Set("syncing", syncing);
  dict.Set("paused", paused);
  dict.Set("syncingHistory", syncingHistory);

  return dict;
}

void TabSearchSyncHandler::HandleGetSyncInfo(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetSyncInfo());
}

base::Value::Dict TabSearchSyncHandler::GetAccountInfo() const {
  const signin::IdentityManager* const identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  const auto stored_account = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  base::Value::Dict dict;
  dict.Set("name", stored_account.full_name);
  dict.Set("email", stored_account.email);
  const auto& avatar_image = stored_account.account_image;
  if (!avatar_image.IsEmpty()) {
    dict.Set("avatarImage", webui::GetBitmapDataUrl(avatar_image.AsBitmap()));
  }
  return dict;
}

void TabSearchSyncHandler::HandleGetAccountInfo(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetAccountInfo());
}

void TabSearchSyncHandler::OnStateChanged(syncer::SyncService* sync_service) {
  FireWebUIListener("sync-info-changed", GetSyncInfo());
}

void TabSearchSyncHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetAccountInfo());
}

void TabSearchSyncHandler::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  FireWebUIListener("account-info-changed", GetAccountInfo());
}

syncer::SyncService* TabSearchSyncHandler::GetSyncService() const {
  return SyncServiceFactory::IsSyncAllowed(profile_)
             ? SyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}
