// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/sync/os_sync_handler.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/web_ui.h"

using syncer::SyncService;
using syncer::SyncUserSettings;
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;

namespace {
const char kWallpaperEnabledKey[] = "wallpaperEnabled";
}  // namespace

OSSyncHandler::OSSyncHandler(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

OSSyncHandler::~OSSyncHandler() {
  RemoveSyncServiceObserver();
  CommitFeatureEnabledPref();
}

void OSSyncHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "DidNavigateToOsSyncPage",
      base::BindRepeating(&OSSyncHandler::HandleDidNavigateToOsSyncPage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "DidNavigateAwayFromOsSyncPage",
      base::BindRepeating(&OSSyncHandler::HandleDidNavigateAwayFromOsSyncPage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "OsSyncPrefsDispatch",
      base::BindRepeating(&OSSyncHandler::HandleOsSyncPrefsDispatch,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SetOsSyncFeatureEnabled",
      base::BindRepeating(&OSSyncHandler::HandleSetOsSyncFeatureEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SetOsSyncDatatypes",
      base::BindRepeating(&OSSyncHandler::HandleSetOsSyncDatatypes,
                          base::Unretained(this)));
}

void OSSyncHandler::OnJavascriptAllowed() {
  AddSyncServiceObserver();
}

void OSSyncHandler::OnJavascriptDisallowed() {
  RemoveSyncServiceObserver();
}

void OSSyncHandler::OnStateChanged(syncer::SyncService* service) {
  if (!is_setting_prefs_)
    PushSyncPrefs();
}

void OSSyncHandler::HandleDidNavigateToOsSyncPage(const base::ListValue* args) {
  HandleOsSyncPrefsDispatch(args);
}

void OSSyncHandler::HandleOsSyncPrefsDispatch(const base::ListValue* args) {
  AllowJavascript();

  // Cache the feature enabled pref.
  SyncService* service = GetSyncService();
  if (service)
    feature_enabled_ = service->GetUserSettings()->IsOsSyncFeatureEnabled();
  PushSyncPrefs();
}

void OSSyncHandler::HandleDidNavigateAwayFromOsSyncPage(
    const base::ListValue* args) {
  CommitFeatureEnabledPref();
}

void OSSyncHandler::HandleSetOsSyncFeatureEnabled(const base::ListValue* args) {
  CHECK_EQ(1u, args->GetSize());
  CHECK(args->GetBoolean(0, &feature_enabled_));
  should_commit_feature_enabled_ = true;
  // Changing the feature enabled state may change toggle state.
  PushSyncPrefs();
}

void OSSyncHandler::HandleSetOsSyncDatatypes(const base::ListValue* args) {
  CHECK_EQ(1u, args->GetSize());
  const base::DictionaryValue* result;
  CHECK(args->GetDictionary(0, &result));

  // Wallpaper sync status is stored directly to the profile's prefs.
  bool wallpaper_synced;
  CHECK(result->GetBoolean(kWallpaperEnabledKey, &wallpaper_synced));
  profile_->GetPrefs()->SetBoolean(chromeos::settings::prefs::kSyncOsWallpaper,
                                   wallpaper_synced);

  // Start configuring the SyncService using the configuration passed to us from
  // the JS layer.
  syncer::SyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just stop.
  if (!service || !service->IsEngineInitialized())
    return;

  bool sync_all_os_types;
  CHECK(result->GetBoolean("syncAllOsTypes", &sync_all_os_types));

  UserSelectableOsTypeSet selected_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    std::string key =
        syncer::GetUserSelectableOsTypeName(type) + std::string("Synced");
    bool sync_value;
    CHECK(result->GetBoolean(key, &sync_value)) << key;
    if (sync_value)
      selected_types.Put(type);
  }

  // Filter out any non-registered types. The WebUI may echo back values from
  // toggles for in-development features hidden by feature flags.
  SyncUserSettings* settings = service->GetUserSettings();
  selected_types.RetainAll(settings->GetRegisteredSelectableOsTypes());

  // Don't send updates back to JS while processing values sent from JS.
  base::AutoReset<bool> reset(&is_setting_prefs_, true);
  settings->SetSelectedOsTypes(sync_all_os_types, selected_types);

  // TODO(jamescook): Add metrics for selected types.
}

void OSSyncHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void OSSyncHandler::CommitFeatureEnabledPref() {
  if (!should_commit_feature_enabled_)
    return;
  SyncService* service = GetSyncService();
  if (!service)
    return;
  service->GetUserSettings()->SetOsSyncFeatureEnabled(feature_enabled_);
  should_commit_feature_enabled_ = false;
}

void OSSyncHandler::PushSyncPrefs() {
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service || !service->IsEngineInitialized())
    return;

  base::DictionaryValue args;
  SyncUserSettings* user_settings = service->GetUserSettings();
  // Tell the UI layer which data types are registered/enabled by the user.
  args.SetBoolean("syncAllOsTypes", user_settings->IsSyncAllOsTypesEnabled());
  UserSelectableOsTypeSet registered_types =
      user_settings->GetRegisteredSelectableOsTypes();
  UserSelectableOsTypeSet selected_types = user_settings->GetSelectedOsTypes();

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    std::string type_name = syncer::GetUserSelectableOsTypeName(type);
    args.SetBoolean(type_name + "Registered", registered_types.Has(type));
    args.SetBoolean(type_name + "Synced", selected_types.Has(type));
  }

  // Wallpaper sync status is fetched from prefs and is considered enabled if
  // all OS types are enabled; this mimics behavior of GetSelectedOsTypes().
  args.SetBoolean(kWallpaperEnabledKey,
                  user_settings->IsSyncAllOsTypesEnabled() ||
                      profile_->GetPrefs()->GetBoolean(
                          chromeos::settings::prefs::kSyncOsWallpaper));

  FireWebUIListener("os-sync-prefs-changed", base::Value(feature_enabled_),
                    args);
}

syncer::SyncService* OSSyncHandler::GetSyncService() const {
  const bool is_sync_allowed = SyncServiceFactory::IsSyncAllowed(profile_);
  return is_sync_allowed ? SyncServiceFactory::GetForProfile(profile_)
                         : nullptr;
}

void OSSyncHandler::AddSyncServiceObserver() {
  // Observe even if sync isn't allowed. IsSyncAllowed() can change mid-session.
  SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (service)
    service->AddObserver(this);
}

void OSSyncHandler::RemoveSyncServiceObserver() {
  SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (service)
    service->RemoveObserver(this);
}
