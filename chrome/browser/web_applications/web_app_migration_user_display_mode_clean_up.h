// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MIGRATION_USER_DISPLAY_MODE_CLEAN_UP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MIGRATION_USER_DISPLAY_MODE_CLEAN_UP_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registry_controller.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

class WebAppSyncBridge;

// # Background
// This is a clean up job for https://crbug.com/1125020.
// When BMO shipped to stable the WebAppMigrationManager migration used the
// wrong user_display_mode for kStandalone bookmark apps that were not locally
// installed. It read kBrowser instead of kStandalone because we always open
// non-locally installed apps in browser tabs. The migrated kBrowser web app
// was then used to populate the sync server which replicated across to all
// other devices. Even if they had kStandalone locally installed bookmark apps
// that migrated correctly our sync logic always chooses the sync server's data
// over the local data. If the first device to migrate and populate the sync
// server hit this bug then all devices would replicate the kBrowser state.
// In effect users would experience their kStandalone window PWAs opening in
// browser tabs instead.
//
// # Clean up pseudocode
// On start up:
//  - Check that we haven't already run this clean up successfully.
//  - Wait for bookmark apps, web apps and sync to be ready.
//  - Check for any kStandalone bookmarks with corresponding kBrowser web apps.
//  - Set those web apps to be kStandalone.
//
// # Known issue
// The migration bug is indistinguishable from a user manually setting the
// user_display_mode for their web apps to kBrowser after migration. This clean
// up CL will erroneously undo such a change. To mitigate this we only run the
// clean up once per migrated device.
class WebAppMigrationUserDisplayModeCleanUp
    : public syncer::SyncServiceObserver {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static std::unique_ptr<WebAppMigrationUserDisplayModeCleanUp> CreateIfNeeded(
      Profile* profile,
      WebAppSyncBridge* sync_bridge);

  static void DisableForTesting();
  static void SkipWaitForSyncForTesting();
  static void SetCompletedCallbackForTesting(base::OnceClosure callback);

  WebAppMigrationUserDisplayModeCleanUp(Profile* profile,
                                        WebAppSyncBridge* sync_bridge);
  WebAppMigrationUserDisplayModeCleanUp(
      const WebAppMigrationUserDisplayModeCleanUp&) = delete;
  WebAppMigrationUserDisplayModeCleanUp& operator=(
      const WebAppMigrationUserDisplayModeCleanUp&) = delete;
  ~WebAppMigrationUserDisplayModeCleanUp() final;

  void Start();
  void Shutdown();

  // syncer::SyncServiceObserver:
  void OnSyncCycleCompleted(syncer::SyncService* sync_service) final;
  void OnSyncShutdown(syncer::SyncService* sync_service) final;

 private:
  void OnBookmarkAppRegistryReady();
  void WaitForFirstSyncCycle(base::OnceClosure callback);
  void OnFirstSyncCycleComplete();

  Profile* profile_ = nullptr;
  WebAppSyncBridge* sync_bridge_ = nullptr;
  syncer::SyncService* sync_service_ = nullptr;
  base::OnceClosure sync_ready_callback_;

  extensions::BookmarkAppRegistrar bookmark_app_registrar_;
  extensions::BookmarkAppRegistryController bookmark_app_registry_controller_;

  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};

  base::WeakPtrFactory<WebAppMigrationUserDisplayModeCleanUp> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MIGRATION_USER_DISPLAY_MODE_CLEAN_UP_H_
