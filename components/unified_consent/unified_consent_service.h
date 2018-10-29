// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service_client.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefChangeRegistrar;
class PrefService;

namespace syncer {
class SyncService;
}

namespace unified_consent {

using Service = UnifiedConsentServiceClient::Service;
using ServiceState = UnifiedConsentServiceClient::ServiceState;

enum class MigrationState : int {
  kNotInitialized = 0,
  kInProgressWaitForSyncInit = 1,
  // Reserve space for other kInProgress* entries to be added here.
  kCompleted = 10,
};

// A browser-context keyed service that is used to manage the user consent
// when UnifiedConsent feature is enabled.
class UnifiedConsentService : public KeyedService,
                              public UnifiedConsentServiceClient::Observer,
                              public identity::IdentityManager::Observer,
                              public syncer::SyncServiceObserver {
 public:
  UnifiedConsentService(
      std::unique_ptr<UnifiedConsentServiceClient> service_client,
      PrefService* pref_service,
      identity::IdentityManager* identity_manager,
      syncer::SyncService* sync_service);
  ~UnifiedConsentService() override;

  // Register the prefs used by this UnifiedConsentService.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Rolls back changes made during migration. This method does nothing if the
  // user hasn't migrated to unified consent yet.
  static void RollbackIfNeeded(PrefService* user_pref_service,
                               syncer::SyncService* sync_service,
                               UnifiedConsentServiceClient* service_client);

  // This updates the consent pref and if |unified_consent_given| is true, all
  // unified consent services are enabled.
  void SetUnifiedConsentGiven(bool unified_consent_given);
  bool IsUnifiedConsentGiven();

  // Returns true if all criteria is met to show the consent bump.
  bool ShouldShowConsentBump();
  // Marks the consent bump as shown. Any future calls to
  // |ShouldShowConsentBump| are guaranteed to return false.
  void MarkConsentBumpShown();
  // Records the consent bump suppress reason and updates the state whether the
  // consent bump should be shown. Note: In some cases, e.g. sync paused,
  // |ShouldShowConsentBump| will still return true.
  void RecordConsentBumpSuppressReason(
      metrics::ConsentBumpSuppressReason suppress_reason);

  // KeyedService:
  void Shutdown() override;

  // UnifiedConsentServiceClient::Observer:
  void OnServiceStateChanged(Service service) override;

  // IdentityManager::Observer:
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

 private:
  friend class UnifiedConsentServiceTest;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Updates the sync settings if sync isn't disabled and the sync engine is
  // initialized.
  // When |sync_everything| is false:
  //  - All sync data types in |enable_data_types| will be enabled.
  //  - All sync data types in |disable_data_types| will be disabled.
  //  - All data types in neither of the two sets will remain in the same state.
  // When |sync_everything| is true, |enable_data_types| and
  // |disable_data_types| will be ignored.
  void UpdateSyncSettingsIfPossible(
      bool sync_everything,
      syncer::ModelTypeSet enable_data_types = syncer::ModelTypeSet(),
      syncer::ModelTypeSet disable_data_types = syncer::ModelTypeSet());

  // Posts a task to call |UpdateSyncSettingsIfPossible|.
  void PostTaskToUpdateSyncSettings(
      bool sync_everything,
      syncer::ModelTypeSet enable_data_types = syncer::ModelTypeSet(),
      syncer::ModelTypeSet disable_data_types = syncer::ModelTypeSet());

  // Called when |prefs::kUnifiedConsentGiven| pref value changes.
  // When set to true, it enables syncing of all data types and it enables all
  // non-personalized services. Otherwise it does nothing.
  void OnUnifiedConsentGivenPrefChanged();

  // Migration helpers.
  MigrationState GetMigrationState();
  void SetMigrationState(MigrationState migration_state);
  // Called when the unified consent service is created. This sets the
  // |kShouldShowUnifiedConsentBump| pref to true if the user is eligible and
  // calls |UpdateSettingsForMigration| at the end.
  void MigrateProfileToUnifiedConsent();
  // Updates the settings preferences for the migration when the sync engine is
  // initialized. When it is not, this function will be called again from
  // |OnStateChanged| when the sync engine is initialized.
  void UpdateSettingsForMigration();

  // Returns true if all non-personalized services are enabled.
  bool AreAllNonPersonalizedServicesEnabled();

  // Checks if all on-by-default non-personalized services are on.
  bool AreAllOnByDefaultPrivacySettingsOn();

  // Records a sample for each bucket enabled by the user (except kNone).
  // kNone is recorded when none of the other buckets are recorded.
  void RecordSettingsHistogram();

  // This method is called on startup to check the eligibility criteria for
  // showing the consent bump. The check is only done when the profile was
  // eligible before. If the user is not eligible anymore, the
  // kShouldShowUnifiedConsentBump pref is set to false.
  void CheckConsentBumpEligibility();

  std::unique_ptr<UnifiedConsentServiceClient> service_client_;
  PrefService* pref_service_;
  identity::IdentityManager* identity_manager_;
  syncer::SyncService* sync_service_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<UnifiedConsentService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentService);
};

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
