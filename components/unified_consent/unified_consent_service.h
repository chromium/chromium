// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace syncer {
class SyncService;
}

namespace unified_consent {

#if BUILDFLAG(IS_CHROMEOS_ASH)
enum class MigrationState : int {
  kNotInitialized = 0,
  kInProgressWaitForSyncInit = 1,
  // Reserve space for other kInProgress* entries to be added here.
  kCompleted = 10,
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A browser-context keyed service that is used to manage the user consent
// when UnifiedConsent feature is enabled.
//
// This service makes sure that UrlKeyedAnonymizedDataCollection is turned on
// during sync opt-in and turned off when the user opts out.
//
// During the advanced opt-in through settings, the changes the user makes to
// the service toggles(prefs) are applied after prefs start syncing. This is
// done to prevent changes the user makes during sync setup to be overridden by
// syncing down older changes.
class UnifiedConsentService
    : public KeyedService,
      public signin::IdentityManager::Observer,
      public syncer::SyncServiceObserver,
      public sync_preferences::PrefServiceSyncableObserver {
 public:
  // Initializes the service. The |service_pref_names| vector is used to track
  // pref changes during the first sync setup.
  UnifiedConsentService(sync_preferences::PrefServiceSyncable* pref_service,
                        signin::IdentityManager* identity_manager,
                        syncer::SyncService* sync_service,
                        const std::vector<std::string>& service_pref_names);

  UnifiedConsentService(const UnifiedConsentService&) = delete;
  UnifiedConsentService& operator=(const UnifiedConsentService&) = delete;

  ~UnifiedConsentService() override;

  // Register the prefs used by this UnifiedConsentService.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Enables or disables URL-keyed anonymized data collection.
  void SetUrlKeyedAnonymizedDataCollectionEnabled(bool enabled);

  // KeyedService:
  void Shutdown() override;

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  friend class UnifiedConsentServiceTest;

  enum class SyncState {
    // The user is not signed in.
    kSignedOut,
    // The user is signed in, but has not opted in to history.
    kSignedInWithoutHistory,
    // The user is signed in and has opted in to history. The passphrase state
    // (explicit passphrase or not) is not known yet - it will be determined
    // once the Sync engine successfully initializes for the first time. This
    // typically happens very soon (< 1s) after sign-in, but in some cases (e.g.
    // no connection to the Sync server possible) can be delayed indefinitely.
    // When entering this state, data collection will get enabled - if it's
    // later determined that there is an explicit passphrase, it'll get disabled
    // again.
    kSignedInWithHistoryWaitingForPassphrase,
    // The user is signed in and has opted in to history, but has an explicit
    // passphrase.
    kSignedInWithHistoryAndExplicitPassphrase,
    // The user is signed in and has opted in to history, and does not have an
    // explicit passphrase.
    kSignedInWithHistoryAndNoPassphrase,
  };

  static SyncState GetSyncState(const syncer::SyncService* sync_service);

  static bool ShouldEnableUrlKeyedAnonymizedDataCollection(
      SyncState old_sync_state,
      SyncState new_sync_state);
  static bool ShouldDisableUrlKeyedAnonymizedDataCollection(
      SyncState old_sync_state,
      SyncState new_sync_state);

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // sync_preferences::PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  // Helpers for observing changes in the service prefs.
  void StartObservingServicePrefChanges();
  void StopObservingServicePrefChanges();
  void ServicePrefChanged(const std::string& name);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Migration helpers.
  MigrationState GetMigrationState();
  void SetMigrationState(MigrationState migration_state);
  // Called when the unified consent service is created.
  void MigrateProfileToUnifiedConsent();
  // Updates the settings preferences for the migration when the sync engine is
  // initialized. When it is not, this function will be called again from
  // |OnStateChanged| when the sync engine is initialized.
  void UpdateSettingsForMigration();
#endif

  raw_ptr<sync_preferences::PrefServiceSyncable> pref_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<syncer::SyncService> sync_service_;

  // Used to monitor changes to the history opt-in and the explicit-passphrase
  // state. Only populated and used if `kReplaceSyncPromosWithSignInPromos` is
  // enabled.
  SyncState last_sync_state_ = SyncState::kSignedOut;

  // Used for tracking the service pref states during the advanced sync opt-in.
  const std::vector<std::string> service_pref_names_;
  std::map<std::string, base::Value> service_pref_changes_;
  PrefChangeRegistrar service_pref_change_registrar_;
};

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
