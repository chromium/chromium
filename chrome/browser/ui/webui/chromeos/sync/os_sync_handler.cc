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
}

void OSSyncHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "DidNavigateToOsSyncPage",
      base::BindRepeating(&OSSyncHandler::HandleDidNavigateToOsSyncPage,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "DidNavigateAwayFromOsSyncPage",
      base::BindRepeating(&OSSyncHandler::HandleDidNavigateAwayFromOsSyncPage,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "OsSyncPrefsDispatch",
      base::BindRepeating(&OSSyncHandler::HandleOsSyncPrefsDispatch,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
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

  PushSyncPrefs();
}

void OSSyncHandler::HandleDidNavigateAwayFromOsSyncPage(
    const base::ListValue* args) {
  // TODO(https://crbug.com/1278325): Remove this.
}

void OSSyncHandler::HandleSetOsSyncDatatypes(const base::ListValue* args) {
  CHECK_EQ(1u, args->GetListDeprecated().size());
  const base::Value& result_value = args->GetListDeprecated()[0];
  CHECK(result_value.is_dict());
  const base::DictionaryValue& result =
      base::Value::AsDictionaryValue(result_value);

  // Wallpaper sync status is stored directly to the profile's prefs.
  bool wallpaper_synced = result.FindBoolPath(kWallpaperEnabledKey).value();
  profile_->GetPrefs()->SetBoolean(chromeos::settings::prefs::kSyncOsWallpaper,
                                   wallpaper_synced);

  // Start configuring the SyncService using the configuration passed to us from
  // the JS layer.
  syncer::SyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just stop.
  if (!service || !service->IsEngineInitialized())
    return;

  bool sync_all_os_types = result.FindBoolKey("syncAllOsTypes").value();

  UserSelectableOsTypeSet selected_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    std::string key =
        syncer::GetUserSelectableOsTypeName(type) + std::string("Synced");
    absl::optional<bool> sync_value = result.FindBoolPath(key);
    CHECK(sync_value.has_value()) << key;
    if (sync_value.value())
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

void OSSyncHandler::PushSyncPrefs() {
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service || !service->IsEngineInitialized())
    return;

  base::DictionaryValue args;
  SyncUserSettings* user_settings = service->GetUserSettings();
  // Tell the UI layer which data types are registered/enabled by the user.
  args.SetBoolKey("syncAllOsTypes", user_settings->IsSyncAllOsTypesEnabled());
  UserSelectableOsTypeSet registered_types =
      user_settings->GetRegisteredSelectableOsTypes();
  UserSelectableOsTypeSet selected_types = user_settings->GetSelectedOsTypes();

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    std::string type_name = syncer::GetUserSelectableOsTypeName(type);
    args.SetBoolPath(type_name + "Registered", registered_types.Has(type));
    args.SetBoolPath(type_name + "Synced", selected_types.Has(type));
  }

  // Wallpaper sync status is fetched from prefs and is considered enabled if
  // all OS types are enabled; this mimics behavior of GetSelectedOsTypes().
  args.SetBoolKey(kWallpaperEnabledKey,
                  user_settings->IsSyncAllOsTypesEnabled() ||
                      profile_->GetPrefs()->GetBoolean(
                          chromeos::settings::prefs::kSyncOsWallpaper));

  FireWebUIListener("os-sync-prefs-changed", args);
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
