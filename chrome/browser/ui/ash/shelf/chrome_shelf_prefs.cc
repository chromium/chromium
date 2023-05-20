// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/prefs_migration_uma.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/default_pinned_apps.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/pref_names.h"
#include "components/app_constants/constants.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

using syncer::UserSelectableOsType;
using syncer::UserSelectableType;

namespace {

// Returns pinned app position even if app is not currently visible on device
// that is leftmost item on the shelf. If |exclude_chrome| is true then Chrome
// app is not processed. if nothing pinned found, returns an invalid ordinal.
syncer::StringOrdinal GetFirstPinnedAppPosition(
    app_list::AppListSyncableService* syncable_service,
    bool exclude_chrome) {
  syncer::StringOrdinal position;
  for (const auto& sync_peer : syncable_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;
    if (exclude_chrome && (sync_peer.first == app_constants::kChromeAppId ||
                           sync_peer.first == app_constants::kLacrosAppId)) {
      continue;
    }
    if (!position.IsValid() ||
        sync_peer.second->item_pin_ordinal.LessThan(position)) {
      position = sync_peer.second->item_pin_ordinal;
    }
  }
  return position;
}

// Helper to create pin position that stays before any synced app, even if
// app is not currently visible on a device.
syncer::StringOrdinal CreateFirstPinPosition(
    app_list::AppListSyncableService* syncable_service) {
  const syncer::StringOrdinal position =
      GetFirstPinnedAppPosition(syncable_service, false /* exclude_chrome */);
  return position.IsValid() ? position.CreateBefore()
                            : syncer::StringOrdinal::CreateInitialOrdinal();
}

// Ensures |app_id| is pinned. If it is not pinned, makes it pinned in the first
// position.
void EnsurePinnedOrMakeFirst(
    const std::string& app_id,
    app_list::AppListSyncableService* syncable_service) {
  syncer::StringOrdinal position = syncable_service->GetPinPosition(app_id);
  if (!position.IsValid()) {
    position = CreateFirstPinPosition(syncable_service);
    syncable_service->SetPinPosition(app_id, position);
  }
}

const char kDefaultPinnedAppsKey[] = "default";

// This is the default prefix for a chrome app loaded in the primary profile in
// Lacros. Note that "Default" is the name of the directory for the main profile
// in Lacros's --user-data-dir. This is fragile, but is hopefully only needed
// for a brief time while transitioning from ash chrome apps to Lacros chrome
// apps. Note that to support multi-profile chrome apps, we're going to need a
// profile-stable identifier anyways, in the near future.
const char kLacrosChromeAppPrefix[] = "Default###";

bool skip_pinned_apps_from_sync_for_test = false;

std::vector<ash::ShelfID> AppIdsToShelfIDs(
    const std::vector<std::string>& app_ids) {
  std::vector<ash::ShelfID> shelf_ids(app_ids.size());
  for (size_t i = 0; i < app_ids.size(); ++i)
    shelf_ids[i] = ash::ShelfID(app_ids[i]);
  return shelf_ids;
}

struct PinInfo {
  PinInfo(const std::string& app_id, const syncer::StringOrdinal& item_ordinal)
      : app_id(app_id), item_ordinal(item_ordinal) {}

  std::string app_id;
  syncer::StringOrdinal item_ordinal;
};

struct ComparePinInfo {
  bool operator()(const PinInfo& pin1, const PinInfo& pin2) {
    return pin1.item_ordinal.LessThan(pin2.item_ordinal);
  }
};

// Helper function that returns the right pref string based on device type.
// This is required because tablet form factor devices do not sync app
// positions and pin preferences.
std::string GetShelfDefaultPinLayoutPref() {
  if (ash::switches::IsTabletFormFactor())
    return prefs::kShelfDefaultPinLayoutRollsForTabletFormFactor;

  return prefs::kShelfDefaultPinLayoutRolls;
}

// Returns true in case default pin layout configuration could be applied
// safely. That means all required components are synced or worked in local
// mode.
bool IsSafeToApplyDefaultPinLayout(Profile* profile) {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  // No |sync_service| in incognito mode.
  if (!sync_service)
    return true;

  // Tablet form-factor devices do not have position sync.
  if (ash::switches::IsTabletFormFactor())
    return true;

  const syncer::SyncUserSettings* settings = sync_service->GetUserSettings();

  // If App sync is not yet started, don't apply default pin apps once synced
  // apps is likely override it. There is a case when App sync is disabled and
  // in last case local cache is available immediately.
  if (sync_service->IsSyncFeatureEnabled() &&
      settings->GetSelectedOsTypes().Has(UserSelectableOsType::kOsApps) &&
      !app_list::AppListSyncableServiceFactory::GetForProfile(profile)
           ->IsSyncing()) {
    return false;
  }

  // If shelf pin layout rolls preference is not started yet then we cannot say
  // if we rolled layout or not.
  if (sync_service->IsSyncFeatureEnabled() &&
      settings->GetSelectedOsTypes().Has(
          UserSelectableOsType::kOsPreferences) &&
      !PrefServiceSyncableFromProfile(profile)->AreOsPrefsSyncing()) {
    return false;
  }
  return true;
}

}  // namespace

const char ChromeShelfPrefs::kPinnedAppsPrefAppIDKey[] = "id";

ChromeShelfPrefs::ChromeShelfPrefs(Profile* profile) : profile_(profile) {}

ChromeShelfPrefs::~ChromeShelfPrefs() {
  StopObservingSyncService();
}

void ChromeShelfPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPolicyPinnedLauncherApps);
  registry->RegisterListPref(
      prefs::kShelfDefaultPinLayoutRolls,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterListPref(
      prefs::kShelfDefaultPinLayoutRollsForTabletFormFactor,
      PrefRegistry::NO_REGISTRATION_FLAGS);
}

void ChromeShelfPrefs::InitLocalPref(PrefService* prefs,
                                     const char* local,
                                     const char* synced) {
  // Ash's prefs *should* have been propagated to Chrome by now, but maybe not.
  // This belongs in Ash, but it can't observe syncing changes: crbug.com/774657
  if (prefs->FindPreference(local) && prefs->FindPreference(synced) &&
      !prefs->FindPreference(local)->HasUserSetting()) {
    prefs->SetString(local, prefs->GetString(synced));
  }
}

// Helper that extracts app list from policy preferences.
std::vector<std::string> ChromeShelfPrefs::GetAppsPinnedByPolicy(
    ShelfControllerHelper* helper) {
  const base::Value::List& policy_apps =
      GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);
  if (policy_apps.empty()) {
    return {};
  }

  std::vector<std::string> policy_entries;
  for (const auto& policy_app : policy_apps) {
    if (!policy_app.is_dict()) {
      continue;
    }
    const std::string* policy_entry = policy_app.GetDict().FindString(
        ChromeShelfPrefs::kPinnedAppsPrefAppIDKey);
    if (!policy_entry) {
      LOG(ERROR) << "Cannot extract policy app info from prefs.";
      continue;
    }

    if (ash::DemoSession::Get() &&
        !ash::DemoSession::Get()->ShouldShowAndroidOrChromeAppInShelf(
            *policy_entry)) {
      continue;
    }

    policy_entries.push_back(apps_util::TransformRawPolicyId(*policy_entry));
  }

  if (policy_entries.empty()) {
    return {};
  }

  std::vector<std::string> results;
  for (const auto& policy_entry : policy_entries) {
    std::vector<std::string> app_ids =
        apps_util::GetAppIdsFromPolicyId(helper->profile(), policy_entry);
    if (app_ids.empty()) {
      LOG(WARNING) << "No matching app(s) found for |policy_entry| = "
                   << policy_entry;
      continue;
    }
    base::Extend(results, std::move(app_ids));
  }

  return results;
}

// Helper to creates pin position that stays before any synced app, even if
// app is not currently visible on a device.
syncer::StringOrdinal CreateLastPinPosition(Profile* profile) {
  syncer::StringOrdinal position;
  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  for (const auto& sync_peer : syncable_service->sync_items()) {
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;
    if (!position.IsValid() ||
        sync_peer.second->item_pin_ordinal.GreaterThan(position)) {
      position = sync_peer.second->item_pin_ordinal;
    }
  }

  return position.IsValid() ? position.CreateAfter()
                            : syncer::StringOrdinal::CreateInitialOrdinal();
}

// Helper to create and insert pins on the shelf for the set of apps defined in
// |app_ids| after Chrome in the first position and before any other pinned app.
// If Chrome is not the first pinned app then apps are pinned before any other
// app.
void InsertPinsAfterChromeAndBeforeFirstPinnedApp(
    app_list::AppListSyncableService* syncable_service,
    const std::vector<std::string>& app_ids) {
  // Chrome must be pinned at this point.
  syncer::StringOrdinal chrome_position =
      syncable_service->GetPinPosition(app_constants::kChromeAppId);
  DCHECK(chrome_position.IsValid());

  // New pins are inserted after this position.
  syncer::StringOrdinal after;
  // New pins are inserted before this position.
  syncer::StringOrdinal before =
      GetFirstPinnedAppPosition(syncable_service, true /* exclude_chrome */);

  if (!before.IsValid()) {
    before = chrome_position.CreateAfter();
  }

  if (before.GreaterThan(chrome_position)) {
    // Perfect case, Chrome is the first pinned app and we have next pinned app.
    after = chrome_position;
  } else {
    after = before.CreateBefore();
  }

  // When chrome and lacros are enabled side-by-side, try positioning default
  // apps after lacros icon.
  if (crosapi::browser_util::IsLacrosEnabled()) {
    syncer::StringOrdinal lacros_position =
        syncable_service->GetPinPosition(app_constants::kLacrosAppId);
    if (lacros_position.IsValid() && lacros_position.LessThan(before) &&
        lacros_position.GreaterThan(after)) {
      after = lacros_position;
    }
  }

  for (const auto& app_id : app_ids) {
    // Check if we already processed the current app.
    if (syncable_service->GetPinPosition(app_id).IsValid())
      continue;

    const syncer::StringOrdinal position = after.CreateBetween(before);
    syncable_service->SetPinPosition(app_id, position);

    // Shift after position, next policy pin position will be created after
    // current item.
    after = position;
  }
}

std::vector<ash::ShelfID> ChromeShelfPrefs::GetPinnedAppsFromSync(
    ShelfControllerHelper* helper) {
  PrefService* prefs = GetPrefs();
  app_list::AppListSyncableService* const syncable_service =
      GetSyncableService();

  // Some unit tests may not have it or service may not be initialized.
  if (!syncable_service || !syncable_service->IsInitialized() ||
      skip_pinned_apps_from_sync_for_test) {
    return std::vector<ash::ShelfID>();
  }

  ObserveSyncService();

  if (ShouldPerformConsistencyMigrations()) {
    needs_consistency_migrations_ = false;
    MigrateFilesChromeAppToSWA(syncable_service);
    EnsureChromePinned(syncable_service);
  }

  // This migration must be run outside of the consistency migrations block
  // since the timing can occur later, after apps have been synced.
  if (!DidAddDefaultApps(prefs) && ShouldAddDefaultApps(prefs)) {
    AddDefaultApps(prefs, syncable_service);
  }

  // Handle pins, forced by policy. In case Chrome is first app they are added
  // after Chrome, otherwise they are added to the front. Note, we handle apps
  // that may not be currently on device. At this case pin position would be
  // preallocated and apps will appear on shelf in deterministic order, even if
  // their install order differ.
  InsertPinsAfterChromeAndBeforeFirstPinnedApp(syncable_service,
                                               GetAppsPinnedByPolicy(helper));

  // If Lacros is enabled and allowed for this user type, ensure the Lacros icon
  // is pinned. Lacros doesn't support multi-signin, so only add the icon for
  // the primary user.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      ash::ProfileHelper::IsPrimaryProfile(profile_)) {
    syncer::StringOrdinal lacros_position =
        syncable_service->GetPinPosition(app_constants::kLacrosAppId);
    if (!lacros_position.IsValid()) {
      // If Lacros isn't already pinned, add it to the right of the Chrome icon.
      InsertPinsAfterChromeAndBeforeFirstPinnedApp(
          syncable_service, {app_constants::kLacrosAppId});
    }
  }

  std::vector<PinInfo> pin_infos;

  // Empty pins indicates that sync based pin model is used for the first
  // time. In the normal workflow we have at least Chrome browser pin info.

  for (const auto& sync_peer : syncable_service->sync_items()) {
    // A null ordinal means the item has been unpinned.
    if (!sync_peer.second->item_pin_ordinal.IsValid())
      continue;

    // kLacrosAppId is only valid when side-by-side Lacros is enabled. When
    // either lacros or ash is the only browser, kChromeAppId is the only valid
    // sync ID for the browser.
    bool lacros_side_by_side = crosapi::browser_util::IsLacrosEnabled() &&
                               crosapi::browser_util::IsAshWebBrowserEnabled();
    if (!lacros_side_by_side && sync_peer.first == app_constants::kLacrosAppId)
      continue;

    std::string app_id = GetShelfId(sync_peer.first);

    // All sync items must be valid app service apps to be added to the shelf
    // with the exception of ash-chrome, which for legacy reasons does not use
    // the app service.
    bool is_ash_chrome = app_id == app_constants::kChromeAppId;
    if (!is_ash_chrome && !helper->IsValidIDForCurrentUser(app_id))
      continue;

    pin_infos.emplace_back(std::move(app_id),
                           sync_peer.second->item_pin_ordinal);
  }

  // Sort pins according their ordinals.
  std::sort(pin_infos.begin(), pin_infos.end(), ComparePinInfo());

  // Convert to ShelfID array.
  std::vector<std::string> pins(pin_infos.size());
  for (size_t i = 0; i < pin_infos.size(); ++i)
    pins[i] = pin_infos[i].app_id;

  return AppIdsToShelfIDs(pins);
}

void ChromeShelfPrefs::RemovePinPosition(Profile* profile,
                                         const ash::ShelfID& shelf_id) {
  DCHECK(profile);

  const std::string& app_id = shelf_id.app_id;
  if (!shelf_id.launch_id.empty()) {
    VLOG(2) << "Syncing remove pin for '" << app_id
            << "' with non-empty launch id '" << shelf_id.launch_id
            << "' is not supported.";
    return;
  }
  DCHECK(!app_id.empty());

  // There currently exists a side-case that only exists in lacros side-by-side
  // where ash-chrome can be unpinned. This won't occur in the long-term because
  // lacros will be the only browser. Until then, explicitly disallow pinning of
  // ash-chrome here. This is simpler than letter the logic leak into various
  // parts of shelf and context menu code.
  if (app_id == app_constants::kChromeAppId) {
    LOG(ERROR) << "ash cannot be unpinned";
    return;
  }

  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  syncable_service->SetPinPosition(app_id, syncer::StringOrdinal());
}

void ChromeShelfPrefs::SetPinPosition(
    const ash::ShelfID& shelf_id,
    const ash::ShelfID& shelf_id_before,
    const std::vector<ash::ShelfID>& shelf_ids_after) {
  const std::string app_id = GetSyncId(shelf_id.app_id);

  // No sync ids should begin with the lacros prefix, as it isn't stable.
  DCHECK(!base::StartsWith(app_id, kLacrosChromeAppPrefix));

  if (!shelf_id.launch_id.empty()) {
    VLOG(2) << "Syncing set pin for '" << app_id
            << "' with non-empty launch id '" << shelf_id.launch_id
            << "' is not supported.";
    return;
  }

  const std::string app_id_before = GetSyncId(shelf_id_before.app_id);

  DCHECK(!app_id.empty());
  DCHECK_NE(app_id, app_id_before);

  app_list::AppListSyncableService* syncable_service = GetSyncableService();
  // Some unit tests may not have this service.
  if (!syncable_service)
    return;

  syncer::StringOrdinal position_before =
      app_id_before.empty() ? syncer::StringOrdinal()
                            : syncable_service->GetPinPosition(app_id_before);
  syncer::StringOrdinal position_after;
  for (const auto& shelf_id_after : shelf_ids_after) {
    std::string app_id_after = GetSyncId(shelf_id_after.app_id);
    DCHECK_NE(app_id_after, app_id);
    DCHECK_NE(app_id_after, app_id_before);
    syncer::StringOrdinal position =
        syncable_service->GetPinPosition(app_id_after);
    DCHECK(position.IsValid());
    if (!position.IsValid()) {
      LOG(ERROR) << "Sync pin position was not found for " << app_id_after;
      continue;
    }
    if (!position_before.IsValid() || !position.Equals(position_before)) {
      position_after = position;
      break;
    }
  }

  syncer::StringOrdinal pin_position;
  if (position_before.IsValid() && position_after.IsValid())
    pin_position = position_before.CreateBetween(position_after);
  else if (position_before.IsValid())
    pin_position = position_before.CreateAfter();
  else if (position_after.IsValid())
    pin_position = position_after.CreateBefore();
  else
    pin_position = syncer::StringOrdinal::CreateInitialOrdinal();
  syncable_service->SetPinPosition(app_id, pin_position);
}

void ChromeShelfPrefs::SkipPinnedAppsFromSyncForTest() {
  skip_pinned_apps_from_sync_for_test = true;
}

void ChromeShelfPrefs::MigrateFilesChromeAppToSWA(
    app_list::AppListSyncableService* syncable_service) {
  if (GetPrefs()->GetBoolean(ash::prefs::kFilesAppUIPrefsMigrated)) {
    return;
  }

  // Avoid migrating the user prefs (even if the migration fails) to avoid
  // overriding preferences that a user may set on the SWA explicitly.
  GetPrefs()->SetBoolean(ash::prefs::kFilesAppUIPrefsMigrated, true);

  using MigrationStatus = file_manager::FileManagerPrefsMigrationStatus;
  if (!syncable_service->GetSyncItem(extension_misc::kFilesManagerAppId)) {
    base::UmaHistogramEnumeration(file_manager::kPrefsMigrationStatusUMA,
                                  MigrationStatus::kFailNoExistingPreferences);
    return;
  }
  if (!syncable_service->TransferItemAttributes(
          /*from_app=*/extension_misc::kFilesManagerAppId,
          /*to_app=*/file_manager::kFileManagerSwaAppId)) {
    base::UmaHistogramEnumeration(file_manager::kPrefsMigrationStatusUMA,
                                  MigrationStatus::kFailMigratingPreferences);
    return;
  }

  base::UmaHistogramEnumeration(file_manager::kPrefsMigrationStatusUMA,
                                MigrationStatus::kSuccess);
}

void ChromeShelfPrefs::EnsureChromePinned(
    app_list::AppListSyncableService* syncable_service) {
  // If ash is the only web browser or if lacros is the only web browser, ensure
  // that ash-chrome is pinned. The sync<->shelf translation layer ensures that
  // we will use the appropriate shelf id.
  if (!crosapi::browser_util::IsLacrosEnabled() ||
      !crosapi::browser_util::IsAshWebBrowserEnabled()) {
    EnsurePinnedOrMakeFirst(app_constants::kChromeAppId, syncable_service);
    return;
  }

  // Otherwise, we are in a transition situation where both the ash and lacros
  // web browsers are available. To ensure consistency with legacy behavior, we
  // ensure both web browsers are pinned.
  EnsurePinnedOrMakeFirst(app_constants::kLacrosAppId, syncable_service);
  EnsurePinnedOrMakeFirst(app_constants::kChromeAppId, syncable_service);
}

bool ChromeShelfPrefs::DidAddDefaultApps(PrefService* pref_service) {
  const auto& layouts_rolled =
      pref_service->GetList(GetShelfDefaultPinLayoutPref());
  return !layouts_rolled.empty();
}

bool ChromeShelfPrefs::ShouldAddDefaultApps(PrefService* pref_service) {
  // Apply default apps in case profile syncing is done. Otherwise there is a
  // risk that applied default apps would be overwritten by sync once it is
  // completed. prefs::kPolicyPinnedLauncherApps overrides any default layout.
  // This also limits applying experimental configuration only for users who
  // have the default pin layout specified by |kDefaultPinnedApps| or for
  // fresh users who have no pin information at all. Default configuration is
  // not applied if any of experimental layout was rolled.
  return !pref_service->HasPrefPath(prefs::kPolicyPinnedLauncherApps) &&
         IsSafeToApplyDefaultPinLayout(profile_);
}

void ChromeShelfPrefs::AddDefaultApps(
    PrefService* pref_service,
    app_list::AppListSyncableService* syncable_service) {
  VLOG(1) << "Roll default shelf pin layout " << kDefaultPinnedAppsKey;
  std::vector<std::string> default_app_ids;
  for (const char* default_app_id : GetDefaultPinnedAppsForFormFactor())
    default_app_ids.push_back(default_app_id);
  InsertPinsAfterChromeAndBeforeFirstPinnedApp(syncable_service,
                                               default_app_ids);
  ScopedListPrefUpdate update(pref_service, GetShelfDefaultPinLayoutPref());
  update->Append(kDefaultPinnedAppsKey);
}

void ChromeShelfPrefs::AttachProfile(Profile* profile) {
  profile_ = profile;
  needs_consistency_migrations_ = true;
  StopObservingSyncService();
}

bool ChromeShelfPrefs::ShouldPerformConsistencyMigrations() const {
  return needs_consistency_migrations_;
}

app_list::AppListSyncableService* ChromeShelfPrefs::GetSyncableService() {
  return app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
}

PrefService* ChromeShelfPrefs::GetPrefs() {
  return profile_->GetPrefs();
}

void ChromeShelfPrefs::ObserveSyncService() {
  if (!observed_sync_service_) {
    observed_sync_service_ = GetSyncableService();
    observed_sync_service_->AddObserverAndStart(this);
  }
}

bool ChromeShelfPrefs::IsStandaloneBrowserPublishingChromeApps() {
  return crosapi::browser_util::IsLacrosChromeAppsEnabled();
}

apps::AppType ChromeShelfPrefs::GetAppType(const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  return proxy->AppRegistryCache().GetAppType(app_id);
}

bool ChromeShelfPrefs::IsAshExtensionApp(const std::string& app_id) {
  bool should_run_in_lacros = false;
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&should_run_in_lacros](const apps::AppUpdate& update) {
        should_run_in_lacros = update.IsPlatformApp().value_or(false);
      });
  return should_run_in_lacros;
}

bool ChromeShelfPrefs::IsAshKeepListApp(const std::string& app_id) {
  return extensions::ExtensionAppRunsInOS(app_id);
}

std::string ChromeShelfPrefs::GetShelfId(const std::string& sync_id) {
  // If Lacros is the only web browser, transform the sync_id kChromeAppId to
  // the shelf id kLacrosAppId.
  if (!crosapi::browser_util::IsAshWebBrowserEnabled() &&
      sync_id == app_constants::kChromeAppId) {
    return app_constants::kLacrosAppId;
  }

  // No sync ids should begin with the lacros prefix, as it isn't stable.
  DCHECK(!base::StartsWith(sync_id, kLacrosChromeAppPrefix));

  // No muxing is necessary.
  if (!apps::ShouldMuxExtensionIds()) {
    return sync_id;
  }

  // If Lacros is not publishing chrome apps, no transformation is necessary.
  if (!IsStandaloneBrowserPublishingChromeApps())
    return sync_id;

  // If this app is on the ash keep list, immediately return it. Even if there's
  // a lacros chrome app that matches this id, we still want to use the ash
  // version.
  if (extensions::ExtensionAppRunsInOS(sync_id))
    return sync_id;

  // All the muxing code can be removed once Ash is past M104.
  std::string transformed_app_id = kLacrosChromeAppPrefix + sync_id;

  // If this is an ash extension app, we add a fixed prefix. This is based on
  // the logic in lacros_extensions_util and is not version-skew stable.
  if (IsAshExtensionApp(sync_id)) {
    return transformed_app_id;
  }

  // Now we have to check if the sync id corresponds to a lacros extension app.
  if (GetAppType(transformed_app_id) ==
      apps::AppType::kStandaloneBrowserChromeApp) {
    return transformed_app_id;
  }

  // This is not an extension app for either ash or Lacros.
  return sync_id;
}

std::string ChromeShelfPrefs::GetSyncId(const std::string& shelf_id) {
  // If Lacros is the only web browser, transform the shelf id kLacrosAppId to
  // the sync_id kChromeAppId.
  if (!crosapi::browser_util::IsAshWebBrowserEnabled() &&
      shelf_id == app_constants::kLacrosAppId) {
    return app_constants::kChromeAppId;
  }

  // No muxing is necessary.
  if (!apps::ShouldMuxExtensionIds()) {
    return shelf_id;
  }

  // All the muxing code can be removed once Ash is past M104.
  // If Lacros is not publishing chrome apps, no transformation is necessary.
  if (!IsStandaloneBrowserPublishingChromeApps())
    return shelf_id;

  // If this is a lacros chrome app, then we must remove the prefix.
  std::string prefix_removed = shelf_id;
  base::ReplaceFirstSubstringAfterOffset(&prefix_removed, /*start_offset=*/0,
                                         kLacrosChromeAppPrefix, "");
  apps::AppType type = GetAppType(shelf_id);
  if (type == apps::AppType::kStandaloneBrowserChromeApp) {
    return prefix_removed;
  }

  // If removing the prefix turns this into an ash chrome app, then we must
  // remove the prefix.
  type = GetAppType(prefix_removed);
  if (type == apps::AppType::kChromeApp) {
    return prefix_removed;
  }

  // Do not remove the prefix otherwise.
  return shelf_id;
}

void ChromeShelfPrefs::OnSyncModelUpdated() {
  needs_consistency_migrations_ = true;
}

void ChromeShelfPrefs::StopObservingSyncService() {
  if (observed_sync_service_) {
    observed_sync_service_->RemoveObserver(this);
    observed_sync_service_ = nullptr;
  }
}
