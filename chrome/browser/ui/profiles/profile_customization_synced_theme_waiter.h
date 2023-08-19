// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_CUSTOMIZATION_SYNCED_THEME_WAITER_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_CUSTOMIZATION_SYNCED_THEME_WAITER_H_

#include "base/dcheck_is_on.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

// Helper class to delay showing the profile customization until the synced
// theme is applied.
// This object is intended for one time use and must be destroyed after
// `callback` is called.
class ProfileCustomizationSyncedThemeWaiter
    : public syncer::SyncServiceObserver,
      public ThemeSyncableService::Observer,
      public ThemeServiceObserver {
 public:
  enum class Outcome {
    // Either a theme was applied (successfully or not) or there was no theme
    // stored on the sync server.
    kSyncSuccess,
    // Theme cannot be synced.
    kSyncCannotStart,
    // Sync passphrase is required.
    kSyncPassphraseRequired,
    // Theme wasn't synced within timeout.
    kTimeout,
  };

  ProfileCustomizationSyncedThemeWaiter(
      syncer::SyncService* sync_service,
      ThemeService* theme_service,
      base::OnceCallback<void(Outcome)> callback);
  ~ProfileCustomizationSyncedThemeWaiter() override;

  // Returns whether theme sync can start (i.e. is not disabled by policy,
  // theme sync is enabled, ...).
  static bool CanThemeSyncStart(syncer::SyncService* sync_service);

  // Notifies `callback_` after a synced theme has been applied or a timeout has
  // expired. Must not be called more than once.
  void Run();

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // ThemeSyncableService::Observer:
  void OnThemeSyncStarted(ThemeSyncableService::ThemeSyncState state) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

 private:
  void OnTimeout();
  // Returns `false` and invokes `callback_` with a corresponding outcome if not
  // all preconditions are fulfilled to start the theme sync.
  bool CheckThemeSyncPreconditions();

  // Invokes `callback_` with `outcome` parameter. Also resets all active
  // observations. `this` may be destroyed after this call.
  void InvokeCallback(Outcome outcome);

  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<ThemeService> theme_service_;
  base::OnceCallback<void(Outcome)> callback_;

#if DCHECK_IS_ON()
  bool is_running_ = false;
#endif  // DCHECK_IS_ON()

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::ScopedObservation<ThemeSyncableService, ThemeSyncableService::Observer>
      theme_sync_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_observation_{this};

  base::WeakPtrFactory<class ProfileCustomizationSyncedThemeWaiter>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_CUSTOMIZATION_SYNCED_THEME_WAITER_H_
