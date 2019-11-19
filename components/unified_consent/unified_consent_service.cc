// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {

UnifiedConsentService::UnifiedConsentService(
    sync_preferences::PrefServiceSyncable* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    const std::vector<std::string>& service_pref_names)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      service_pref_names_(service_pref_names) {
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  DCHECK(sync_service_);

  if (GetMigrationState() == MigrationState::kNotInitialized)
    MigrateProfileToUnifiedConsent();

  pref_service_->AddObserver(this);
  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);
}

UnifiedConsentService::~UnifiedConsentService() {}

// static
void UnifiedConsentService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                                false);
  registry->RegisterIntegerPref(
      prefs::kUnifiedConsentMigrationState,
      static_cast<int>(MigrationState::kNotInitialized));
}

void UnifiedConsentService::SetUrlKeyedAnonymizedDataCollectionEnabled(
    bool enabled) {
  if (GetMigrationState() != MigrationState::kCompleted)
    SetMigrationState(MigrationState::kCompleted);

  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            enabled);
}

void UnifiedConsentService::Shutdown() {
  pref_service_->RemoveObserver(this);
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const CoreAccountInfo& account_info) {
  // By design, clearing the primary account disables URL-keyed data collection.
  SetUrlKeyedAnonymizedDataCollectionEnabled(false);
}

void UnifiedConsentService::OnStateChanged(syncer::SyncService* sync) {
  // Start observing pref changes when the user enters sync setup.
  // Note: Only |sync->IsSetupInProgress()| is used (i.e. no check for
  // |IsFirstSetupComplete()|), because on Android |SetFirstSetupComplete()| is
  // called automatically during the first setup, i.e. the value could change in
  // the meantime.
  if (sync->IsSetupInProgress() && !pref_service_->IsSyncing()) {
    StartObservingServicePrefChanges();
  } else {
    StopObservingServicePrefChanges();

    // If the user cancelled the sync setup, clear all observed changes.
    if (!sync->CanSyncFeatureStart())
      service_pref_changes_.clear();
  }

  if (!sync_service_->CanSyncFeatureStart() ||
      !sync_service_->IsEngineInitialized()) {
    return;
  }

  if (GetMigrationState() == MigrationState::kInProgressWaitForSyncInit)
    UpdateSettingsForMigration();
}

void UnifiedConsentService::OnIsSyncingChanged() {
  if (pref_service_->IsSyncing() && !service_pref_changes_.empty()) {
    // Re-apply all observed service pref changes.
    // If any service prefs had a value coming in through Sync, then that
    // would've overridden any changes that the user made during the first sync
    // setup. So re-apply the local changes to make sure they stick.
    for (const auto& pref_change : service_pref_changes_) {
      pref_service_->Set(pref_change.first, pref_change.second);
    }
    service_pref_changes_.clear();
  }
}

void UnifiedConsentService::StartObservingServicePrefChanges() {
  if (!service_pref_change_registrar_.IsEmpty())
    return;

  service_pref_change_registrar_.Init(pref_service_);
  for (const std::string& pref_name : service_pref_names_) {
    service_pref_change_registrar_.Add(
        pref_name,
        base::BindRepeating(&UnifiedConsentService::ServicePrefChanged,
                            base::Unretained(this)));
  }
}

void UnifiedConsentService::StopObservingServicePrefChanges() {
  service_pref_change_registrar_.RemoveAll();
}

void UnifiedConsentService::ServicePrefChanged(const std::string& name) {
  DCHECK(sync_service_->IsSetupInProgress());
  const base::Value* value = pref_service_->Get(name);
  DCHECK(value);
  service_pref_changes_[name] = value->Clone();
}

MigrationState UnifiedConsentService::GetMigrationState() {
  int migration_state_int =
      pref_service_->GetInteger(prefs::kUnifiedConsentMigrationState);
  DCHECK_LE(static_cast<int>(MigrationState::kNotInitialized),
            migration_state_int);
  DCHECK_GE(static_cast<int>(MigrationState::kCompleted), migration_state_int);
  return static_cast<MigrationState>(migration_state_int);
}

void UnifiedConsentService::SetMigrationState(MigrationState migration_state) {
  pref_service_->SetInteger(prefs::kUnifiedConsentMigrationState,
                            static_cast<int>(migration_state));
}

void UnifiedConsentService::MigrateProfileToUnifiedConsent() {
  DCHECK_EQ(GetMigrationState(), MigrationState::kNotInitialized);

  if (!identity_manager_->HasPrimaryAccount()) {
    SetMigrationState(MigrationState::kCompleted);
    return;
  }

  UpdateSettingsForMigration();
}

void UnifiedConsentService::UpdateSettingsForMigration() {
  if (!sync_service_->IsEngineInitialized()) {
    SetMigrationState(MigrationState::kInProgressWaitForSyncInit);
    return;
  }

  // Set URL-keyed anonymized metrics to the state it had before unified
  // consent.
  bool url_keyed_metrics_enabled =
      sync_service_->IsSyncFeatureEnabled() &&
      sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory) &&
      !sync_service_->GetUserSettings()->IsUsingSecondaryPassphrase();
  SetUrlKeyedAnonymizedDataCollectionEnabled(url_keyed_metrics_enabled);
}

}  //  namespace unified_consent
