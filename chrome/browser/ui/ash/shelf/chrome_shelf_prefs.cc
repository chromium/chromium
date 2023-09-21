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
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/prefs_migration_uma.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/default_pinned_apps.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/pref_names.h"
#include "components/app_constants/constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "extensions/common/constants.h"

namespace {

// Returns pinned app position even if app is not currently visible on device
// that is leftmost item on the shelf. If |exclude_chrome| is true then Chrome
// app is not processed. if nothing pinned found, returns an invalid ordinal.
syncer::StringOrdinal GetFirstPinnedAppPosition(
    app_list::AppListSyncableService* syncable_service,
    bool exclude_chrome) {
  syncer::StringOrdinal position;
  for (const auto& [item_id, sync_item] : syncable_service->sync_items()) {
    if (!sync_item->item_pin_ordinal.IsValid()) {
      continue;
    }
    if (exclude_chrome && (item_id == app_constants::kChromeAppId ||
                           item_id == app_constants::kLacrosAppId ||
                           item_id == app_constants::kAshDebugBrowserAppId)) {
      continue;
    }
    if (!position.IsValid() || sync_item->item_pin_ordinal.LessThan(position)) {
      position = sync_item->item_pin_ordinal;
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
  // This piece prevents accidental side-effects to the SetPinPosition() call
  // below.
  CHECK(app_id == app_constants::kChromeAppId ||
        app_id == app_constants::kLacrosAppId);
  syncer::StringOrdinal position = syncable_service->GetPinPosition(app_id);
  if (!position.IsValid()) {
    position = CreateFirstPinPosition(syncable_service);
    syncable_service->SetPinPosition(app_id, position,
                                     /*is_policy_initiated=*/false);
  }
}

constexpr char kDefaultPinnedAppsKey[] = "default";

bool skip_pinned_apps_from_sync_for_test = false;

bool should_add_default_apps_for_test = false;

struct PinInfo {
  PinInfo(const std::string& app_id, const syncer::StringOrdinal& item_ordinal)
      : app_id(app_id), item_ordinal(item_ordinal) {}

  std::string app_id;
  syncer::StringOrdinal item_ordinal;
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
      settings->GetSelectedOsTypes().Has(
          syncer::UserSelectableOsType::kOsApps) &&
      !app_list::AppListSyncableServiceFactory::GetForProfile(profile)
           ->IsSyncing()) {
    return false;
  }

  // If shelf pin layout rolls preference is not started yet then we cannot say
  // if we rolled layout or not.
  if (sync_service->IsSyncFeatureEnabled() &&
      settings->GetSelectedOsTypes().Has(
          syncer::UserSelectableOsType::kOsPreferences) &&
      !PrefServiceSyncableFromProfile(profile)->AreOsPrefsSyncing()) {
    return false;
  }
  return true;
}

bool IsOnlyPolicyPinned(app_list::AppListSyncableService::SyncItem* sync_item) {
  return sync_item->is_user_pinned.has_value() &&
         !sync_item->is_user_pinned.value() &&
         ash::features::IsRemoveStalePolicyPinnedAppsFromShelfEnabled();
}

// In order to ensure that the chrome icon in the shelf is consistent across
// devices, we must apply the following rules:
// (1) If lacros is the only web-browser (lacros_only), transform [sync id]
// kChromeAppId <-> [shelf id] kLacrosAppId
// (2) If lacros is the only web-browser and ash debug browser is enabled,
// transform [sync id] kAshDebugBrowserAppId <-> [shelf id] kChromeAppId
std::string GetShelfId(const std::string& sync_id) {
  if (!crosapi::browser_util::IsAshWebBrowserEnabled()) {
    if (sync_id == app_constants::kChromeAppId) {
      return app_constants::kLacrosAppId;
    }
    if (ash::switches::IsAshDebugBrowserEnabled() &&
        sync_id == app_constants::kAshDebugBrowserAppId) {
      return app_constants::kChromeAppId;
    }
  }

  return sync_id;
}

// In order to ensure that the chrome icon in the shelf is consistent across
// devices, we must apply the following rules:
// (1) If lacros is the only web-browser (lacros_only), transform [shelf id]
// kLacrosAppId <-> [sync id] kChromeAppId
// (2) If lacros is the only web-browser and ash debug browser is enabled,
// transform [shelf id] kChromeAppId <-> [sync id] kAshDebugBrowserAppId
std::string GetSyncId(const std::string& shelf_id) {
  if (!crosapi::browser_util::IsAshWebBrowserEnabled()) {
    if (shelf_id == app_constants::kLacrosAppId) {
      return app_constants::kChromeAppId;
    }
    if (ash::switches::IsAshDebugBrowserEnabled() &&
        shelf_id == app_constants::kChromeAppId) {
      return app_constants::kAshDebugBrowserAppId;
    }
  }

  return shelf_id;
}

}  // namespace

ChromeShelfPrefs::ChromeShelfPrefs(Profile* profile) : profile_(profile) {}

ChromeShelfPrefs::~ChromeShelfPrefs() = default;

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
    Profile* profile) {
  CHECK(profile);
  const base::Value::List& policy_apps =
      profile->GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);
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
        apps_util::GetAppIdsFromPolicyId(profile, policy_entry);
    if (app_ids.empty()) {
      LOG(WARNING) << "No matching app(s) found for |policy_entry| = "
                   << policy_entry;
      continue;
    }
    base::Extend(results, std::move(app_ids));
  }

  return results;
}

// Helper to create and insert pins on the shelf for the set of apps defined in
// |app_ids| after Chrome in the first position and before any other pinned app.
// If Chrome is not the first pinned app then apps are pinned before any other
// app.
void InsertPinsAfterChromeAndBeforeFirstPinnedApp(
    app_list::AppListSyncableService* syncable_service,
    const std::vector<std::string>& app_ids,
    bool is_policy_initiated) {
  // Chrome must be pinned at this point.
  syncer::StringOrdinal chrome_position =
      syncable_service->GetPinPosition(app_constants::kChromeAppId);
  DCHECK(chrome_position.IsValid());

  // New pins are inserted after this position.
  syncer::StringOrdinal after;
  // New pins are inserted before this position.
  syncer::StringOrdinal before =
      GetFirstPinnedAppPosition(syncable_service, /*exclude_chrome=*/true);

  if (!before.IsValid()) {
    before = chrome_position.CreateAfter();
  }

  if (before.GreaterThan(chrome_position)) {
    // Perfect case, Chrome is the first pinned app and we have next pinned app.
    after = chrome_position;
  } else {
    after = before.CreateBefore();
  }

  for (const auto& app_id : app_ids) {
    // Check if we already processed the current app.
    auto* sync_item = syncable_service->GetSyncItem(app_id);
    if (sync_item && sync_item->item_pin_ordinal.IsValid()) {
      // If `is_user_pinned` is currently unknown but the incoming pin is
      // triggered by a change to policy, set `is_user_pinned` to false.
      if (is_policy_initiated && !sync_item->is_user_pinned.has_value() &&
          ash::features::IsRemoveStalePolicyPinnedAppsFromShelfEnabled()) {
        syncable_service->SetIsPolicyPinned(app_id);
      }
      continue;
    }
    const syncer::StringOrdinal position = after.CreateBetween(before);
    syncable_service->SetPinPosition(app_id, position, is_policy_initiated);

    // Shift after position, next policy pin position will be created after
    // current item.
    after = position;
  }
}

std::vector<ash::ShelfID> ChromeShelfPrefs::GetPinnedAppsFromSync(
    ShelfControllerHelper* helper) {
  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  // Some unit tests may not have it or service may not be initialized.
  if (!syncable_service || !syncable_service->IsInitialized() ||
      skip_pinned_apps_from_sync_for_test) {
    return std::vector<ash::ShelfID>();
  }

  if (!sync_service_observer_.IsObserving()) {
    sync_service_observer_.Observe(syncable_service);
  }

  if (ShouldPerformConsistencyMigrations()) {
    needs_consistency_migrations_ = false;
    MigrateFilesChromeAppToSWA();
    EnsureChromePinned();
    EnsureProjectorShelfPinConsistency();
  }

  // This migration must be run outside of the consistency migrations block
  // since the timing can occur later, after apps have been synced.
  if (!DidAddDefaultApps() && ShouldAddDefaultApps()) {
    AddDefaultApps();
  }

  // Handle pins, forced by policy. In case Chrome is first app they are added
  // after Chrome, otherwise they are added to the front. Note, we handle apps
  // that may not be currently on device. At this case pin position would be
  // preallocated and apps will appear on shelf in deterministic order, even if
  // their install order differ.
  std::vector<std::string> policy_pinned_apps = GetAppsPinnedByPolicy(profile_);
  InsertPinsAfterChromeAndBeforeFirstPinnedApp(syncable_service,
                                               policy_pinned_apps,
                                               /*is_policy_initiated=*/true);

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
          syncable_service, {app_constants::kLacrosAppId},
          /*is_policy_initiated=*/false);
    }
  }

  std::vector<std::string> policy_delta_remove_from_shelf;

  std::vector<PinInfo> pin_infos;

  // Empty pins indicates that sync based pin model is used for the first
  // time. In the normal workflow we have at least Chrome browser pin info.
  for (const auto& [item_id, sync_item] : syncable_service->sync_items()) {
    // A null ordinal means the item has been unpinned.
    if (!sync_item->item_pin_ordinal.IsValid()) {
      continue;
    }

    // kChromeAppId is the only valid sync ID for the browser.
    if (item_id == app_constants::kLacrosAppId) {
      continue;
    }

    std::string app_id = GetShelfId(item_id);
    std::string promise_package_id = sync_item->promise_package_id;

    // All sync items must be valid app service apps to be added to the shelf
    // with the exception of ash-chrome, which for legacy reasons does not use
    // the app service.
    bool is_ash_chrome = app_id == app_constants::kChromeAppId;
    if (!is_ash_chrome && !helper->IsValidIDForCurrentUser(app_id) &&
        !helper->IsValidPromisePackageIdFromAppService(promise_package_id)) {
      continue;
    }

    // Prune apps that used to be policy-pinned (`is_user_pinned = false`), but
    // are not a part of the policy anymore.
    if (!is_ash_chrome && IsOnlyPolicyPinned(sync_item.get()) &&
        !base::Contains(policy_pinned_apps, item_id)) {
      policy_delta_remove_from_shelf.push_back(item_id);
      continue;
    }
    pin_infos.emplace_back(std::move(app_id), sync_item->item_pin_ordinal);
  }

  for (const auto& item_id : policy_delta_remove_from_shelf) {
    syncable_service->RemovePinPosition(item_id);
  }

  // Sort pins according their ordinals.
  base::ranges::sort(pin_infos, syncer::StringOrdinal::LessThanFn(),
                     &PinInfo::item_ordinal);

  // Convert to ShelfID array.
  std::vector<ash::ShelfID> pins;
  base::ranges::transform(
      pin_infos, std::back_inserter(pins),
      [](const auto& pin_info) { return ash::ShelfID(pin_info.app_id); });

  return pins;
}

void ChromeShelfPrefs::RemovePinPosition(const ash::ShelfID& shelf_id) {
  DCHECK(profile_);

  const std::string& app_id = shelf_id.app_id;
  if (!shelf_id.launch_id.empty()) {
    VLOG(2) << "Syncing remove pin for '" << app_id
            << "' with non-empty launch id '" << shelf_id.launch_id
            << "' is not supported.";
    return;
  }
  DCHECK(!app_id.empty());
  app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
      ->RemovePinPosition(app_id);
}

void ChromeShelfPrefs::SetPinPosition(
    const ash::ShelfID& shelf_id,
    const ash::ShelfID& shelf_id_before,
    const std::vector<ash::ShelfID>& shelf_ids_after,
    bool pinned_by_policy) {
  const std::string app_id = GetSyncId(shelf_id.app_id);

  if (!shelf_id.launch_id.empty()) {
    VLOG(2) << "Syncing set pin for '" << app_id
            << "' with non-empty launch id '" << shelf_id.launch_id
            << "' is not supported.";
    return;
  }

  const std::string app_id_before = GetSyncId(shelf_id_before.app_id);

  DCHECK(!app_id.empty());
  DCHECK_NE(app_id, app_id_before);

  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
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
  syncable_service->SetPinPosition(app_id, pin_position, pinned_by_policy);
}

void ChromeShelfPrefs::SkipPinnedAppsFromSyncForTest() {
  skip_pinned_apps_from_sync_for_test = true;
}

void ChromeShelfPrefs::SetShouldAddDefaultAppsForTest() {
  should_add_default_apps_for_test = true;
}

void ChromeShelfPrefs::MigrateFilesChromeAppToSWA() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->GetBoolean(ash::prefs::kFilesAppUIPrefsMigrated)) {
    return;
  }

  // Avoid migrating the user prefs (even if the migration fails) to avoid
  // overriding preferences that a user may set on the SWA explicitly.
  prefs->SetBoolean(ash::prefs::kFilesAppUIPrefsMigrated, true);

  using MigrationStatus = file_manager::FileManagerPrefsMigrationStatus;
  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  if (!syncable_service->GetSyncItem(extension_misc::kFilesManagerAppId)) {
    base::UmaHistogramEnumeration(file_manager::kPrefsMigrationStatusUMA,
                                  MigrationStatus::kFailNoExistingPreferences);
    return;
  }
  if (!syncable_service->TransferItemAttributes(
          /*from_app_id=*/extension_misc::kFilesManagerAppId,
          /*to_app_id=*/file_manager::kFileManagerSwaAppId)) {
    base::UmaHistogramEnumeration(file_manager::kPrefsMigrationStatusUMA,
                                  MigrationStatus::kFailMigratingPreferences);
    return;
  }

  base::UmaHistogramEnumeration(file_manager::kPrefsMigrationStatusUMA,
                                MigrationStatus::kSuccess);
}

void ChromeShelfPrefs::EnsureProjectorShelfPinConsistency() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->GetBoolean(ash::prefs::kProjectorSWAUIPrefsMigrated)) {
    return;
  }

  prefs->SetBoolean(ash::prefs::kProjectorSWAUIPrefsMigrated, true);
  app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
      ->TransferItemAttributes(ash::kChromeUITrustedProjectorSwaAppIdDeprecated,
                               ash::kChromeUIUntrustedProjectorSwaAppId);
}

void ChromeShelfPrefs::EnsureChromePinned() {
  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
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

bool ChromeShelfPrefs::DidAddDefaultApps() const {
  const auto& layouts_rolled =
      profile_->GetPrefs()->GetList(GetShelfDefaultPinLayoutPref());
  return !layouts_rolled.empty();
}

bool ChromeShelfPrefs::ShouldAddDefaultApps() const {
  if (should_add_default_apps_for_test) {
    return true;
  }
  // Apply default apps in case profile syncing is done. Otherwise there is a
  // risk that applied default apps would be overwritten by sync once it is
  // completed. prefs::kPolicyPinnedLauncherApps overrides any default layout.
  // This also limits applying experimental configuration only for users who
  // have the default pin layout specified by |kDefaultPinnedApps| or for
  // fresh users who have no pin information at all. Default configuration is
  // not applied if any of experimental layout was rolled.
  return !profile_->GetPrefs()->HasPrefPath(prefs::kPolicyPinnedLauncherApps) &&
         IsSafeToApplyDefaultPinLayout(profile_);
}

void ChromeShelfPrefs::AddDefaultApps() {
  VLOG(1) << "Roll default shelf pin layout " << kDefaultPinnedAppsKey;
  std::vector<std::string> default_app_ids;
  for (const char* default_app_id : GetDefaultPinnedAppsForFormFactor())
    default_app_ids.push_back(default_app_id);
  InsertPinsAfterChromeAndBeforeFirstPinnedApp(
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_),
      default_app_ids, /*is_policy_initiated=*/false);
  ScopedListPrefUpdate update(profile_->GetPrefs(),
                              GetShelfDefaultPinLayoutPref());
  update->Append(kDefaultPinnedAppsKey);
}

void ChromeShelfPrefs::AttachProfile(Profile* profile) {
  profile_ = profile;
  needs_consistency_migrations_ = true;
  sync_service_observer_.Reset();
}

std::string ChromeShelfPrefs::GetPromisePackageIdForSyncItem(
    const std::string& app_id) {
  if (!ash::features::ArePromiseIconsEnabled()) {
    return std::string();
  }

  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  // Some unit tests may not have the service or it may not be initialized.
  if (!syncable_service || !syncable_service->IsInitialized() ||
      skip_pinned_apps_from_sync_for_test) {
    return std::string();
  }

  const app_list::AppListSyncableService::SyncItem* item =
      syncable_service->GetSyncItem(app_id);
  return item->promise_package_id;
}

bool ChromeShelfPrefs::ShouldPerformConsistencyMigrations() const {
  return needs_consistency_migrations_;
}

void ChromeShelfPrefs::OnSyncModelUpdated() {
  needs_consistency_migrations_ = true;
}
