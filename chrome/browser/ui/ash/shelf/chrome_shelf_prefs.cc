// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/webui/mall/app_id.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/file_manager/prefs_migration_uma.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/default_pinned_apps/default_pinned_apps.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/constants/chromeos_features.h"
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

// Returns a result after `lhs` and before `rhs` if they are valid, else returns
// initial-ordinal.
syncer::StringOrdinal CreateBetween(const syncer::StringOrdinal& lhs,
                                    const syncer::StringOrdinal& rhs) {
  if (lhs.IsValid() && rhs.IsValid()) {
    return lhs.CreateBetween(rhs);
  }
  if (lhs.IsValid()) {
    return lhs.CreateAfter();
  }
  if (rhs.IsValid()) {
    return rhs.CreateBefore();
  }
  return syncer::StringOrdinal::CreateInitialOrdinal();
}

// Template for GetNextPositionAfter() and GetNextPositionBefore().
// Returns the adjacent pin (before or after based on `compare` to `position`.
// If |exclude_chrome| is true then Chrome app is not processed. Returns invalid
// if `position` is not found or has no adjacent item.
template <typename Compare>
syncer::StringOrdinal GetAdjacentPosition(
    app_list::AppListSyncableService* syncable_service,
    const syncer::StringOrdinal& position,
    bool exclude_chrome,
    Compare compare) {
  syncer::StringOrdinal result;
  for (const auto& [item_id, sync_item] : syncable_service->sync_items()) {
    if (!sync_item->item_pin_ordinal.IsValid()) {
      continue;
    }
    if (exclude_chrome && (item_id == app_constants::kChromeAppId ||
                           item_id == app_constants::kLacrosAppId ||
                           item_id == app_constants::kAshDebugBrowserAppId)) {
      continue;
    }
    if (position.IsValid() && !compare(position, sync_item->item_pin_ordinal)) {
      continue;
    }

    if (!result.IsValid() || compare(sync_item->item_pin_ordinal, result)) {
      result = sync_item->item_pin_ordinal;
    }
  }
  return result;
}

// Returns the next pin after `position`.
syncer::StringOrdinal GetNextPositionAfter(
    app_list::AppListSyncableService* syncable_service,
    const syncer::StringOrdinal& position,
    bool exclude_chrome = false) {
  return GetAdjacentPosition(
      syncable_service, position, exclude_chrome,
      [](const syncer::StringOrdinal& a, const syncer::StringOrdinal& b) {
        return a.LessThan(b);
      });
}

// Returns the next pin before `position`.
syncer::StringOrdinal GetNextPositionBefore(
    app_list::AppListSyncableService* syncable_service,
    const syncer::StringOrdinal& position,
    bool exclude_chrome = false) {
  return GetAdjacentPosition(
      syncable_service, position, exclude_chrome,
      [](const syncer::StringOrdinal& a, const syncer::StringOrdinal& b) {
        return a.GreaterThan(b);
      });
}

// Returns the last pin position.
syncer::StringOrdinal GetLastPosition(
    app_list::AppListSyncableService* syncable_service) {
  syncer::StringOrdinal invalid;
  return GetNextPositionBefore(syncable_service, invalid);
}

// Returns pinned app position even if app is not currently visible on device
// that is leftmost item on the shelf. If |exclude_chrome| is true then Chrome
// app is not processed. if nothing pinned found, returns an invalid ordinal.
syncer::StringOrdinal GetFirstPinnedAppPosition(
    app_list::AppListSyncableService* syncable_service,
    bool exclude_chrome) {
  syncer::StringOrdinal invalid;
  return GetNextPositionAfter(syncable_service, invalid, exclude_chrome);
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

// Returns pin position of app matching `package_id`.
syncer::StringOrdinal GetAppPosition(
    Profile* profile,
    apps::PackageId package_id,
    app_list::AppListSyncableService* syncable_service) {
  std::optional<std::string> app_id =
      apps_util::GetAppWithPackageId(profile, package_id);
  if (!app_id) {
    return syncer::StringOrdinal();
  }
  return syncable_service->GetPinPosition(*app_id);
}

constexpr char kDefaultPinnedAppsKey[] = "default";
constexpr char kPreloadPinnedAppsKey[] = "preload";

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
  if (!sync_service) {
    return true;
  }

  // Tablet form-factor devices do not have position sync.
  if (ash::switches::IsTabletFormFactor()) {
    return true;
  }

  // Some browser tests don't start sync fully as there is no server to download
  // the initial data from. This prevents applying the default pin layout,
  // required in some tests. To support this, the behavior can be overridden via
  // command-line flags.
  if (ash::switches::ShouldAllowDefaultShelfPinLayoutIgnoringSync()) {
    return true;
  }

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

// Helper to create and insert pins on the shelf for the set of apps defined in
// |app_ids| after Chrome in the first position and before any other pinned app.
// If Chrome is not the first pinned app then apps are pinned before any other
// app.
void InsertPinsAfterChromeAndBeforeFirstPinnedApp(
    app_list::AppListSyncableService* syncable_service,
    base::span<const std::string> app_ids,
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

void AddContainerAppPinIfNeeded(
    Profile* profile,
    ShelfControllerHelper* helper,
    app_list::AppListSyncableService* syncable_service) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!chromeos::features::IsContainerAppPreinstallEnabled()) {
    return;
  }

  if (!profile->GetPrefs()
           ->GetList(prefs::kShelfContainerAppPinRolls)
           .empty()) {
    return;
  }

  const std::string app_id = web_app::kContainerAppId;

  if (!helper->IsAppDefaultInstalled(profile, app_id)) {
    return;
  }

  const app_list::AppListSyncableService::SyncItem* sync_item =
      syncable_service->GetSyncItem(app_id);
  if (sync_item && sync_item->item_pin_ordinal.IsValid()) {
    if (sync_item->is_user_pinned.value_or(true)) {
      ScopedListPrefUpdate update(profile->GetPrefs(),
                                  prefs::kShelfContainerAppPinRolls);
      update->Append("v1");
    }
    return;
  }

  // Pin the container app before chrome.
  syncable_service->SetPinPosition(app_id,
                                   CreateFirstPinPosition(syncable_service),
                                   /*is_policy_initiated=*/false);
  {
    ScopedListPrefUpdate update(profile->GetPrefs(),
                                prefs::kShelfContainerAppPinRolls);
    update->Append("v1");
  }
#endif  // GOOGLE_CHROME_BRANDING
}

// Ensures the Mall app is pinned to the shelf after Chrome, when Mall is
// enabled.
void AddMallPinIfNeeded(Profile* profile,
                        app_list::AppListSyncableService* syncable_service) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kCrosMall)) {
    return;
  }

  // When Mall SWA is disabled, always pin the Mall web app if it is not
  // already pinned. There is no option to unpin the web app.
  if (!chromeos::features::IsCrosMallSwaEnabled()) {
    InsertPinsAfterChromeAndBeforeFirstPinnedApp(syncable_service,
                                                 {{web_app::kMallAppId}},
                                                 /*is_policy_initiated=*/false);
    return;
  }

  // When Mall SWA is enabled, pin the Mall SWA once, and use a synced pref to
  // make sure it doesn't pin a second time. Users have the option to unpin the
  // SWA.
  if (!profile->GetPrefs()->GetList(prefs::kShelfMallAppPinRolls).empty()) {
    return;
  }

  if (!ShelfControllerHelper::IsAppDefaultInstalled(profile,
                                                    ash::kMallSystemAppId)) {
    return;
  }

  InsertPinsAfterChromeAndBeforeFirstPinnedApp(syncable_service,
                                               {{ash::kMallSystemAppId}},
                                               /*is_policy_initiated=*/false);

  ScopedListPrefUpdate update(profile->GetPrefs(),
                              prefs::kShelfMallAppPinRolls);
  update->Append("v1");
}

void SetPreloadPinComplete(Profile* profile) {
  ScopedListPrefUpdate update(profile->GetPrefs(),
                              GetShelfDefaultPinLayoutPref());
  update->Append(kPreloadPinnedAppsKey);
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
      prefs::kShelfContainerAppPinRolls,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterListPref(
      prefs::kShelfDefaultPinLayoutRollsForTabletFormFactor,
      PrefRegistry::NO_REGISTRATION_FLAGS);
  registry->RegisterListPref(
      prefs::kShelfMallAppPinRolls,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

// TODO(crbug.com/350769496): Fixes bug from M127 beta, can be removed once M127
// is no longer in stable (end of 2024, or mid 2025 is ok).
void ChromeShelfPrefs::CleanupPreloadPrefs(PrefService* profile_prefs) {
  constexpr std::array<const char*, 2> kPrefNames{
      prefs::kShelfDefaultPinLayoutRolls,
      prefs::kShelfDefaultPinLayoutRollsForTabletFormFactor};

  for (auto* const pref_name : kPrefNames) {
    // Deduplicate items in list.
    ScopedListPrefUpdate list(profile_prefs, pref_name);
    std::set<base::Value> set;
    for (const auto& item : *list) {
      set.insert(item.Clone());
    }
    if (set.size() < list->size()) {
      list->clear();
      for (const auto& item : set) {
        list->Append(item.Clone());
      }
    }
  }
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
        !ash::DemoSession::Get()->ShouldShowAppInShelf(*policy_entry)) {
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

std::vector<ash::ShelfID> ChromeShelfPrefs::GetPinnedAppsFromSync(
    ShelfControllerHelper* helper) {
  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  // Some unit tests may not have it or service or helper may not be
  // initialized.
  if (!syncable_service || !syncable_service->IsInitialized() || !helper) {
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

  if (IsSafeToApplyDefaultPinLayout(profile_)) {
    AddContainerAppPinIfNeeded(profile_, helper, syncable_service);
    AddMallPinIfNeeded(profile_, syncable_service);
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

  // Pin preload apps only if none are set by policy.
  if (!DidAddPreloadApps() && policy_pinned_apps.empty() &&
      IsSafeToApplyDefaultPinLayout(profile_)) {
    PinPreloadApps();
  }

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
          syncable_service, {{app_constants::kLacrosAppId}},
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

    // All sync items must be valid app service apps to be added to the shelf
    // with the exception of ash-chrome, which for legacy reasons does not use
    // the app service.
    bool is_ash_chrome = app_id == app_constants::kChromeAppId;
    if (!is_ash_chrome && !helper->IsValidIDForCurrentUser(app_id) &&
        !ShelfControllerHelper::IsPromiseApp(profile_, app_id)) {
      continue;
    }

    // Prune apps that used to be policy-pinned (`is_user_pinned = false`), but
    // are not a part of the policy anymore.
    if (!is_ash_chrome && IsOnlyPolicyPinned(sync_item.get()) &&
        !base::Contains(policy_pinned_apps, item_id) &&
        !ShelfControllerHelper::IsPromiseApp(profile_, app_id)) {
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
    base::span<const ash::ShelfID> shelf_ids_after,
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

  syncer::StringOrdinal pin_position =
      CreateBetween(position_before, position_after);
  syncable_service->SetPinPosition(app_id, pin_position, pinned_by_policy);
}

void ChromeShelfPrefs::SetShouldAddDefaultAppsForTest(bool value) {
  should_add_default_apps_for_test = value;
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
  return base::Contains(
      profile_->GetPrefs()->GetList(GetShelfDefaultPinLayoutPref()),
      kDefaultPinnedAppsKey);
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
  for (const char* default_app_id :
       GetDefaultPinnedAppsForFormFactor(profile_)) {
    default_app_ids.push_back(default_app_id);
  }
  InsertPinsAfterChromeAndBeforeFirstPinnedApp(
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_),
      default_app_ids, /*is_policy_initiated=*/false);
  ScopedListPrefUpdate update(profile_->GetPrefs(),
                              GetShelfDefaultPinLayoutPref());
  update->Append(kDefaultPinnedAppsKey);
}

bool ChromeShelfPrefs::DidAddPreloadApps() const {
  return base::Contains(
      profile_->GetPrefs()->GetList(GetShelfDefaultPinLayoutPref()),
      kPreloadPinnedAppsKey);
}

void ChromeShelfPrefs::PinPreloadApps() {
  // Only pin once per user.
  if (pending_preload_apps_.empty() || DidAddPreloadApps()) {
    return;
  }

  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  for (auto it = pending_preload_apps_.begin();
       it != pending_preload_apps_.end();) {
    // If app is not installed yet, check again later, else delete it from the
    // pending list.
    apps::PackageId package_id = *it;
    std::optional<std::string> app_id =
        apps_util::GetAppWithPackageId(profile_, package_id);
    if (!app_id) {
      ++it;
      continue;
    }
    it = pending_preload_apps_.erase(it);

    // Ignore if already pinned.
    if (syncable_service->GetPinPosition(*app_id).IsValid()) {
      LOG(WARNING) << "Preload already pinned " << package_id;
      continue;
    }

    // Place this app between lhs and rhs, or last if we don't find a match.
    syncer::StringOrdinal lhs = GetLastPosition(syncable_service);
    syncer::StringOrdinal rhs;

    // Find app then search in reverse to find the first app that exists prior
    // to this app in desired order. If none found, then search forward for the
    // first app that exists after in desired order.
    size_t i = 0;
    for (; i < preload_pin_order_.size(); i++) {
      if (preload_pin_order_[i] == package_id) {
        break;
      }
    }
    if (i == preload_pin_order_.size()) {
      LOG(ERROR) << "Preload pin app not found in pin order " << package_id;
      continue;
    }
    size_t app_index = i;
    // Find closest prior app and pin after it.
    for (i = app_index; i > 0; i--) {
      apps::PackageId app = preload_pin_order_[i - 1];
      auto pos = GetAppPosition(profile_, app, syncable_service);
      if (pos.IsValid()) {
        lhs = pos;
        rhs = GetNextPositionAfter(syncable_service, pos);
        break;
      }
    }
    // If no prior app, then find next subsequent app and pin before it.
    if (i == 0) {
      for (i = app_index + 1; i < preload_pin_order_.size(); i++) {
        apps::PackageId app = preload_pin_order_[i];
        auto pos = GetAppPosition(profile_, app, syncable_service);
        if (pos.IsValid()) {
          rhs = pos;
          lhs = GetNextPositionBefore(syncable_service, pos);
          break;
        }
      }
    }
    syncer::StringOrdinal position = CreateBetween(lhs, rhs);
    syncable_service->SetPinPosition(*app_id, position,
                                     /*is_policy_initiated=*/false);
  }

  // Mark preload pin complete once all apps are installed and pinned.
  if (pending_preload_apps_.empty()) {
    SetPreloadPinComplete(profile_);
  }
}

void ChromeShelfPrefs::AttachProfile(Profile* profile) {
  profile_ = profile;
  needs_consistency_migrations_ = true;
  sync_service_observer_.Reset();
  if (profile_) {
    CleanupPreloadPrefs(profile_->GetPrefs());
  }

  pending_preload_apps_.clear();
  preload_pin_order_.clear();
  if (profile_ && !DidAddPreloadApps()) {
    if (auto* app_preload_service = apps::AppPreloadService::Get(profile_)) {
      app_preload_service->GetPinApps(
          base::BindOnce(&ChromeShelfPrefs::OnGetPinPreloadApps,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

std::string ChromeShelfPrefs::GetPromisePackageIdForSyncItem(
    const std::string& app_id) {
  if (!ash::features::ArePromiseIconsEnabled()) {
    return std::string();
  }

  auto* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  // Some unit tests may not have the service or it may not be initialized.
  if (!syncable_service || !syncable_service->IsInitialized()) {
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

void ChromeShelfPrefs::OnGetPinPreloadApps(
    const std::vector<apps::PackageId>& pin_apps,
    const std::vector<apps::PackageId>& pin_order) {
  pending_preload_apps_ = pin_apps;
  preload_pin_order_ = pin_order;
  if (pin_apps.empty() && !DidAddPreloadApps()) {
    SetPreloadPinComplete(profile_);
  }
}
