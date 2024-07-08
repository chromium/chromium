// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_PREFS_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_PREFS_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "components/services/app_service/public/cpp/package_id.h"

class ShelfControllerHelper;
class PrefService;
class Profile;

namespace ash {
struct ShelfID;
}  // namespace ash

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// This class is responsible for briding between sync/pref-store and the ash
// shelf.
class ChromeShelfPrefs : public app_list::AppListSyncableService::Observer {
 public:
  explicit ChromeShelfPrefs(Profile* profile);
  ChromeShelfPrefs(const ChromeShelfPrefs&) = delete;
  ChromeShelfPrefs& operator=(const ChromeShelfPrefs&) = delete;
  ~ChromeShelfPrefs() override;

  // Key for the dictionary entries in the prefs::kPinnedLauncherApps list
  // specifying the extension ID of the app to be pinned by that entry.
  static constexpr char kPinnedAppsPrefAppIDKey[] = "id";

  // All prefs must be registered early in the process lifecycle.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Cleanup multiple values of 'preload' added to ShelfDefaultPinLayoutRolls.
  static void CleanupPreloadPrefs(PrefService* profile_prefs);

  // Init a local pref from a synced pref, if the local pref has no user
  // setting. This is used to init shelf alignment and auto-hide on the first
  // user sync. The goal is to apply the last elected shelf alignment and
  // auto-hide values when a user signs in to a new device for the first time.
  // Otherwise, shelf properties are persisted per-display/device. The local
  // prefs are initialized with synced (or default) values when when syncing
  // begins, to avoid syncing shelf prefs across devices after the very start of
  // the user's first session.
  void InitLocalPref(PrefService* prefs, const char* local, const char* synced);

  // Gets the ordered list of pinned apps that exist on device from the app sync
  // service. This returns apps that are known to the app service with one
  // exceptions: app_constants::kChromeAppId (ash-chrome) can be returned and is
  // not known to the app service.
  std::vector<ash::ShelfID> GetPinnedAppsFromSync(
      ShelfControllerHelper* helper);

  // Gets the ordered list of apps that have been pinned by policy. May contain
  // duplicates.
  static std::vector<std::string> GetAppsPinnedByPolicy(Profile* profile);

  // Removes information about pin position from sync model for the app.
  // Note, |shelf_id| with non-empty launch_id is not supported.
  void RemovePinPosition(const ash::ShelfID& shelf_id);

  // Updates information about pin position in sync model for the app
  // |shelf_id|. |shelf_id_before| optionally specifies an app that exists right
  // before the target app. |shelf_ids_after| optionally specifies sorted by
  // position apps that exist right after the target app. Note, |shelf_id| with
  // non-empty launch_id is not supported.
  // |pinned_by_policy| tells whether this item is pinned to the shelf by the
  // `PinnedLauncherApps` policy.
  void SetPinPosition(const ash::ShelfID& shelf_id,
                      const ash::ShelfID& shelf_id_before,
                      base::span<const ash::ShelfID> shelf_ids_after,
                      bool pinned_by_policy);

  // Makes ShouldAddDefaultApps() return true if set to true.
  static void SetShouldAddDefaultAppsForTest(bool value);

  // In multi-user login, it's possible for the profile to change during a
  // session. This requires resetting all migrations. This method is also called
  // shorty after initialization.
  void AttachProfile(Profile* profile);

  // Get the `promise_package_id` value if one exists for the sync item with an
  // item ID of `app_id`.
  std::string GetPromisePackageIdForSyncItem(const std::string& app_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, AddChromePinNoExistingOrdinal);
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, AddChromePinExistingOrdinal);
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, AddDefaultApps);
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, ProfileChanged);
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, LacrosOnlyPinnedApp);
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, PinPreloadApps);
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, PinPreloadRepeats);
  FRIEND_TEST_ALL_PREFIXES(ChromeShelfPrefsTest, PinPreloadEmpty);

  // Sync is the source of truth. However, the data from sync can be
  // nonsensical, either because the user nuked all sync data, corruption, or
  // otherwise. The purpose of the consistency migrations are two fold:
  //   (1) To perform what should logically be 1-time migrations.
  //   (2) In case of data loss or corruption, to bring the user back to a
  //   safe/consistent state.
  // By necessity, all consistency migrations must be idempotent. Otherwise the
  // logic will trigger on every call to GetPinnedAppsFromSync().
  //
  // This method returns whether the consistency migrations need to be run
  // again.
  bool ShouldPerformConsistencyMigrations() const;

  // Ensure the Files Chrome app pinned positions are appropriately migrated to
  // the Files System Web App.
  void MigrateFilesChromeAppToSWA();

  // Ensure that Projector app pinned positions are appropriatley migrated after
  // the change to its app-id.
  void EnsureProjectorShelfPinConsistency();

  // This is run each time ash launches and each time new data is obtained from
  // sync. It ensures that both ash-chrome and lacros-chrome are properly
  // pinned or unpinned.
  void EnsureChromePinned();

  // Whether the default apps have already been added for this device form
  // factor.
  bool DidAddDefaultApps() const;

  // Whether it's safe to add the default apps. We will refrain from adding the
  // default apps if there are policies that modify the pinned apps, or if app
  // sync has not yet started.
  bool ShouldAddDefaultApps() const;

  // This migration is run once per device form factor and the result is stored
  // in prefs. It is never run again if that pref is present. It causes several
  // default apps to be shown in the shelf.
  void AddDefaultApps();

  // Whether App Preload Service apps have already been added.
  bool DidAddPreloadApps() const;

  // Pin preload apps received along with desired order via OnAppPreloadReady().
  void PinPreloadApps();

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override;

  // Receives Pin preload apps when AppPreloadService is ready.
  void OnGetPinPreloadApps(const std::vector<apps::PackageId>& pin_apps,
                           const std::vector<apps::PackageId>& pin_order);

  // Migrations are performed in several situations:
  //   (1) On first launch
  //   (2) Any time there's a sync update
  //   (3) On profile change.
  // In order to prevent an endless cycle of sync updates, all migrations must
  // be idempotent.
  bool needs_consistency_migrations_ = true;

  // List of preload apps to pin once they are installed.
  std::vector<apps::PackageId> pending_preload_apps_;

  // Desired order to pin preload apps, includes default apps (e.g. chrome) to
  // show where preload apps should be pinned.
  std::vector<apps::PackageId> preload_pin_order_;

  base::ScopedObservation<app_list::AppListSyncableService,
                          app_list::AppListSyncableService::Observer>
      sync_service_observer_{this};

  // The owner of this class is responsible for ensuring the validity of this
  // pointer.
  raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtrFactory<ChromeShelfPrefs> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_PREFS_H_
