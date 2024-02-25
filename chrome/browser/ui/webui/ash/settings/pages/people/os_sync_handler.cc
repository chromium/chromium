// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/os_sync_handler.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

using syncer::SyncService;
using syncer::SyncUserSettings;
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;

namespace ash {

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
      "SetOsSyncDatatypes",
      base::BindRepeating(&OSSyncHandler::HandleSetOsSyncDatatypes,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "OpenBrowserSyncSettings",
      base::BindRepeating(&OSSyncHandler::HandleOpenBrowserSyncSettings,
                          base::Unretained(this)));
}

void OSSyncHandler::OnJavascriptAllowed() {
  AddSyncServiceObserver();
}

void OSSyncHandler::OnJavascriptDisallowed() {
  RemoveSyncServiceObserver();
}

void OSSyncHandler::OnStateChanged(syncer::SyncService* service) {
  PushSyncPrefs();
}

void OSSyncHandler::HandleDidNavigateToOsSyncPage(
    const base::Value::List& args) {
  HandleOsSyncPrefsDispatch(args);
}

void OSSyncHandler::HandleOsSyncPrefsDispatch(const base::Value::List& args) {
  AllowJavascript();

  PushSyncPrefs();
}

void OSSyncHandler::HandleDidNavigateAwayFromOsSyncPage(
    const base::Value::List& args) {
  // TODO(https://crbug.com/1278325): Remove this.
}

void OSSyncHandler::HandleOpenBrowserSyncSettings(
    const base::Value::List& args) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kSyncSetupSubPage),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}

void OSSyncHandler::HandleSetOsSyncDatatypes(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  const base::Value& result_value = args[0];
  CHECK(result_value.is_dict());
  const base::Value::Dict& result = result_value.GetDict();

  // Wallpaper sync status is stored directly to the profile's prefs.
  bool wallpaper_synced = result.FindBool(kWallpaperEnabledKey).value();
  profile_->GetPrefs()->SetBoolean(settings::prefs::kSyncOsWallpaper,
                                   wallpaper_synced);

  // Start configuring the SyncService using the configuration passed to us from
  // the JS layer.
  syncer::SyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just stop.
  if (!service || !service->IsEngineInitialized()) {
    return;
  }

  bool sync_all_os_types = result.FindBool("syncAllOsTypes").value();

  UserSelectableOsTypeSet selected_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    std::string key =
        syncer::GetUserSelectableOsTypeName(type) + std::string("Synced");
    std::optional<bool> sync_value = result.FindBool(key);
    CHECK(sync_value.has_value()) << key;
    if (sync_value.value()) {
      selected_types.Put(type);
    }
  }

  // Filter out any non-registered types. The WebUI may echo back values from
  // toggles for in-development features hidden by feature flags.
  SyncUserSettings* settings = service->GetUserSettings();
  selected_types.RetainAll(settings->GetRegisteredSelectableOsTypes());

  settings->SetSelectedOsTypes(sync_all_os_types, selected_types);

  // TODO(jamescook): Add metrics for selected types.
}

void OSSyncHandler::PushSyncPrefs() {
  syncer::SyncService* service = GetSyncService();
  // The sync service may be nullptr if it has been just disabled by policy.
  if (!service || !service->IsEngineInitialized()) {
    return;
  }

  base::Value::Dict args;
  SyncUserSettings* user_settings = service->GetUserSettings();
  // Tell the UI layer which data types are registered/enabled by the user.
  args.Set("syncAllOsTypes", user_settings->IsSyncAllOsTypesEnabled());
  UserSelectableOsTypeSet registered_types =
      user_settings->GetRegisteredSelectableOsTypes();
  UserSelectableOsTypeSet selected_types = user_settings->GetSelectedOsTypes();

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    std::string type_name = syncer::GetUserSelectableOsTypeName(type);
    args.SetByDottedPath(type_name + "Registered", registered_types.Has(type));
    args.SetByDottedPath(type_name + "Synced", selected_types.Has(type));
  }

  // Wallpaper sync status is fetched from prefs and is considered enabled if
  // all OS types are enabled; this mimics behavior of GetSelectedOsTypes().
  args.Set(kWallpaperEnabledKey, user_settings->IsSyncAllOsTypesEnabled() ||
                                     profile_->GetPrefs()->GetBoolean(
                                         settings::prefs::kSyncOsWallpaper));

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
  if (service) {
    service->AddObserver(this);
  }
}

void OSSyncHandler::RemoveSyncServiceObserver() {
  SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (service) {
    service->RemoveObserver(this);
  }
}

}  // namespace ash
