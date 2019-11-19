// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/sync/os_sync_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/web_ui.h"

using syncer::SyncService;
using syncer::SyncUserSettings;
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;

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
  PushSyncPrefs();
}

void OSSyncHandler::HandleDidNavigateToOsSyncPage(const base::ListValue* args) {
  AllowJavascript();

  SyncService* service = GetSyncService();

  if (service && !sync_blocker_)
    sync_blocker_ = service->GetSetupInProgressHandle();

  // TODO(jamescook): Sort out consent/opt-in story, then SetSyncRequested()
  // here.

  PushSyncPrefs();
}

void OSSyncHandler::HandleDidNavigateAwayFromOsSyncPage(
    const base::ListValue* args) {
  sync_blocker_.reset();
}

void OSSyncHandler::HandleSetOsSyncDatatypes(const base::ListValue* args) {
  CHECK_EQ(1u, args->GetSize());
  const base::DictionaryValue* result;
  CHECK(args->GetDictionary(0, &result));

  // Start configuring the SyncService using the configuration passed to us from
  // the JS layer.
  syncer::SyncService* service = GetSyncService();

  // If the sync engine has shutdown for some reason, just stop.
  if (!service || !service->IsEngineInitialized()) {
    sync_blocker_.reset();
    return;
  }

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
  settings->SetSelectedOsTypes(sync_all_os_types, selected_types);

  // Update the enabled state last so that the selected types will be set before
  // pref observers are notified of the change.
  bool feature_enabled;
  CHECK(result->GetBoolean("featureEnabled", &feature_enabled));
  settings->SetOsSyncFeatureEnabled(feature_enabled);

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
  args.SetBoolean("featureEnabled", user_settings->GetOsSyncFeatureEnabled());
  // Tell the UI layer which data types are registered/enabled by the user.
  UserSelectableOsTypeSet registered_types =
      user_settings->GetRegisteredSelectableOsTypes();
  UserSelectableOsTypeSet selected_types = user_settings->GetSelectedOsTypes();

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    std::string type_name = syncer::GetUserSelectableOsTypeName(type);
    args.SetBoolean(type_name + "Registered", registered_types.Has(type));
    args.SetBoolean(type_name + "Synced", selected_types.Has(type));
    // TODO(crbug.com/1020236): Add SyncUserSettings::GetForcedOsTypes() if we
    // decide to support Apps sync for supervised users.
    args.SetBoolean(type_name + "Enforced", false);
  }
  args.SetBoolean("syncAllOsTypes", user_settings->IsSyncAllOsTypesEnabled());
  FireWebUIListener("os-sync-prefs-changed", args);
}

syncer::SyncService* OSSyncHandler::GetSyncService() const {
  return profile_->IsSyncAllowed()
             ? ProfileSyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void OSSyncHandler::AddSyncServiceObserver() {
  // Observe even if sync isn't allowed. IsSyncAllowed() can change mid-session.
  SyncService* service = ProfileSyncServiceFactory::GetForProfile(profile_);
  if (service)
    service->AddObserver(this);
}

void OSSyncHandler::RemoveSyncServiceObserver() {
  SyncService* service = ProfileSyncServiceFactory::GetForProfile(profile_);
  if (service)
    service->RemoveObserver(this);
}
