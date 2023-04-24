// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_PREFS_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_PREFS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "components/services/app_service/public/cpp/app_types.h"

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
  static const char kPinnedAppsPrefAppIDKey[];

  // All prefs must be registered early in the process lifecycle.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

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
  std::vector<std::string> GetAppsPinnedByPolicy(ShelfControllerHelper* helper);

  // Removes information about pin position from sync model for the app.
  // Note, |shelf_id| with non-empty launch_id is not supported.
  void RemovePinPosition(Profile* profile, const ash::ShelfID& shelf_id);

  // Updates information about pin position in sync model for the app
  // |shelf_id|. |shelf_id_before| optionally specifies an app that exists right
  // before the target app. |shelf_ids_after| optionally specifies sorted by
  // position apps that exist right after the target app. Note, |shelf_id| with
  // non-empty launch_id is not supported.
  void SetPinPosition(const ash::ShelfID& shelf_id,
                      const ash::ShelfID& shelf_id_before,
                      const std::vector<ash::ShelfID>& shelf_ids_after);

  // Makes GetPinnedAppsFromSync() return an empty list. Avoids test failures
  // due to an untitled Play Store icon in the shelf.
  // https://crbug.com/1085597
  static void SkipPinnedAppsFromSyncForTest();

  // Ensure the Files Chrome app pinned positions are appropriately migrated to
  // the Files System Web App.
  void MigrateFilesChromeAppToSWA(
      app_list::AppListSyncableService* syncable_service);

  // This is run each time ash launches and each time new data is obtained from
  // sync. It ensures that both ash-chrome and lacros-chrome are properly
  // pinned or unpinned.
  void EnsureChromePinned(app_list::AppListSyncableService* syncable_service);

  // Whether the default apps have already been added for this device form
  // factor.
  bool DidAddDefaultApps(PrefService* pref_service);

  // Virtual for testing.
  // Whether it's safe to add the default apps. We will refrain from adding the
  // default apps if there are policies that modify the pinned apps, or if app
  // sync has not yet started.
  virtual bool ShouldAddDefaultApps(PrefService* pref_service);

  // This migration is run once per device form factor and the result is stored
  // in prefs. It is never run again if that pref is present. It causes several
  // default apps to be shown in the shelf.
  void AddDefaultApps(PrefService* pref_service,
                      app_list::AppListSyncableService* syncable_service);

  // In multi-user login, it's possible for the profile to change during a
  // session. This requires resetting all migrations. This method is also called
  // shorty after initialization.
  void AttachProfile(Profile* profile);

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

  // During Lacros development, there is a period of time when we wish to deploy
  // a transparent migration to Lacros, while still allowing users to fall back
  // to Ash. This requires us to be very careful about how we store data in
  // sync, which will be used by potentially both Lacros and Ash. We use the
  // following scheme:
  // (1) If the app is either an ash extension platform app or a lacros
  // extension platform app, we store the ash extension app id in sync.
  // (2) If the app is part of a small keep-list that continues to run in ash,
  // we expose the ash extension app id to the shelf.
  // (3) If Lacros chrome apps is enabled, we expose the lacros extension app id
  // to the shelf.
  // (4) If ash chrome apps is enabled, we expose the ash extension app id to
  // the shelf.
  //
  // These methods are public as there are some places that need to translate
  // from the ShelfId to SyncId to match up with policy, which uses SyncId.
  //
  // In order to ensure that the chrome icon in the shelf is consistent across
  // devices, we must apply the following rules:
  // (1) If ash is the only web-browser, transform [sync id] kChromeAppId <->
  // [shelf id] kChromeAppId
  // (2) If lacros is the only web-browser, transform [sync id] kChromeAppId <->
  // [shelf id] kLacrosAppId
  // (3) If lacros and ash are both web browsers, do not use any transformation.
  std::string GetShelfId(const std::string& sync_id);
  std::string GetSyncId(const std::string& shelf_id);

 protected:
  // Virtual for testing. Returns the syncable service associated with the
  // current profile.
  virtual app_list::AppListSyncableService* GetSyncableService();

  // Virtual for testing. Returns the pref service associated with the current
  // profile.
  virtual PrefService* GetPrefs();

  // Starts observing the sync service if not already doing so.
  virtual void ObserveSyncService();

  // Virtual for testing. The migration to use a standalone browser (lacros) to
  // publish chrome apps is incomplete. In the interim, this class uses some
  // workarounds to ensure that sync does not end up in an inconsistent state.
  virtual bool IsStandaloneBrowserPublishingChromeApps();

  // Virtual for testing. Returns the app type associated with an app id.
  virtual apps::AppType GetAppType(const std::string& app_id);

  // Virtual for testing. Returns whether this app_id corresponds to an ash
  // extension-based platform app.
  virtual bool IsAshExtensionApp(const std::string& app_id);

  // Virtual for testing. There's a small set of apps that always run in Ash.
  virtual bool IsAshKeepListApp(const std::string& app_id);

 private:
  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override;

  // Stops observing the current sync service.
  void StopObservingSyncService();

  // Migrations are performed in several situations:
  //   (1) On first launch
  //   (2) Any time there's a sync update
  //   (3) On profile change.
  // In order to prevent an endless cycle of sync updates, all migrations must
  // be idempotent.
  bool needs_consistency_migrations_ = true;

  // The sync service instance that is currently being observed. If nullptr then
  // nothing is being observed.
  raw_ptr<app_list::AppListSyncableService, ExperimentalAsh>
      observed_sync_service_ = nullptr;

  // The owner of this class is responsible for ensuring the validity of this
  // pointer.
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_PREFS_H_
