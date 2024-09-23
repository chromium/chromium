// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "base/check_op.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"

namespace unified_consent {

// static
UnifiedConsentService::SyncState UnifiedConsentService::GetSyncState(
    const syncer::SyncService* sync_service) {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));

  if (sync_service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
    return SyncState::kSignedOut;
  }

  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory)) {
    return SyncState::kSignedInWithoutHistory;
  }

  std::optional<syncer::PassphraseType> passphrase_type =
      sync_service->GetUserSettings()->GetPassphraseType();

  if (!passphrase_type.has_value()) {
    return SyncState::kSignedInWithHistoryWaitingForPassphrase;
  }

  if (syncer::IsExplicitPassphrase(*passphrase_type)) {
    return SyncState::kSignedInWithHistoryAndExplicitPassphrase;
  }

  return SyncState::kSignedInWithHistoryAndNoPassphrase;
}

// static
bool UnifiedConsentService::ShouldEnableUrlKeyedAnonymizedDataCollection(
    SyncState old_sync_state,
    SyncState new_sync_state) {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));

  // If nothing changed, leave UrlKeyedAnonymizedDataCollection alone.
  if (old_sync_state == new_sync_state) {
    return false;
  }

  // UrlKeyedAnonymizedDataCollection is only ever automatically enabled when
  // entering the history-on-without-passphrase or -waiting-for-passphrase
  // state.
  switch (new_sync_state) {
    case SyncState::kSignedOut:
    case SyncState::kSignedInWithoutHistory:
    case SyncState::kSignedInWithHistoryAndExplicitPassphrase:
      return false;
    case SyncState::kSignedInWithHistoryWaitingForPassphrase:
    case SyncState::kSignedInWithHistoryAndNoPassphrase:
      // UrlKeyedAnonymizedDataCollection can maybe be enabled, depending on
      // `old_sync_state`.
      // Note that `kSignedInWithHistoryWaitingForPassphrase` will *usually*
      // be entered from `kSignedOut`, but it's not the only possibility: It's
      // also possible to get there from any of the "signed-in" state, e.g. if
      // the user reset their sync data via the dashboard.
      break;
  }

  switch (old_sync_state) {
    case SyncState::kSignedOut:
    case SyncState::kSignedInWithoutHistory:
      // History got turned on, and there's no explicit passphrase, so
      // UrlKeyedAnonymizedDataCollection can be enabled.
      return true;
    case SyncState::kSignedInWithHistoryWaitingForPassphrase:
    case SyncState::kSignedInWithHistoryAndNoPassphrase:
      // UrlKeyedAnonymizedDataCollection would've been turned on based on the
      // old state already, so nothing to do here.
      return false;
    case SyncState::kSignedInWithHistoryAndExplicitPassphrase:
      // Explicit-passphrase was turned off, and history is on,
      // UrlKeyedAnonymizedDataCollection can be enabled.
      // Note that while turning off an explicit passphrase involves resetting
      // all the server-side data and starting over fresh, this process is not
      // expressed in the SyncState.
      return true;
  }
}

// static
bool UnifiedConsentService::ShouldDisableUrlKeyedAnonymizedDataCollection(
    SyncState old_sync_state,
    SyncState new_sync_state) {
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));

  // If nothing changed, leave UrlKeyedAnonymizedDataCollection alone.
  if (old_sync_state == new_sync_state) {
    return false;
  }

  // UrlKeyedAnonymizedDataCollection only ever needs to be automatically
  // disabled if it was previously automatically enabled, which means the old
  // state was history-on-without-passphrase (or -waiting-for-passphrase) state.
  switch (old_sync_state) {
    case SyncState::kSignedOut:
    case SyncState::kSignedInWithoutHistory:
    case SyncState::kSignedInWithHistoryAndExplicitPassphrase:
      return false;
    case SyncState::kSignedInWithHistoryWaitingForPassphrase:
    case SyncState::kSignedInWithHistoryAndNoPassphrase:
      // UrlKeyedAnonymizedDataCollection might have to be disabled, depending
      // on `new_sync_state`.
      break;
  }

  switch (new_sync_state) {
    case SyncState::kSignedOut:
      // User signed out; UrlKeyedAnonymizedDataCollection should be disabled.
      return true;
    case SyncState::kSignedInWithoutHistory:
      // History was turned off, UrlKeyedAnonymizedDataCollection should be
      // disabled.
      return true;
    case SyncState::kSignedInWithHistoryAndNoPassphrase:
      // Passphrase state became known, and there's no explicit passphrase.
      // Nothing to be done.
      return false;
    case SyncState::kSignedInWithHistoryWaitingForPassphrase:
      // Nothing to do - wait until the passphrase state becomes known.
      // Note that this transition (from `kSignedInWithHistoryAndNoPassphrase`
      // to `kSignedInWithHistoryWaitingForPassphrase`) is not typically
      // possible, but it can happen if the local sync (meta)data gets reset and
      // sync starts over.
      return false;
    case SyncState::kSignedInWithHistoryAndExplicitPassphrase:
      // If explicit-passphrase was turned on, UrlKeyedAnonymizedDataCollection
      // should be disabled.
      return true;
  }
}

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetMigrationState() == MigrationState::kNotInitialized)
    MigrateProfileToUnifiedConsent();
#endif

  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    last_sync_state_ = GetSyncState(sync_service_);
  }

  pref_service_->AddObserver(this);
  identity_manager_->AddObserver(this);
  sync_service_->AddObserver(this);
}

UnifiedConsentService::~UnifiedConsentService() = default;

// static
void UnifiedConsentService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                                false);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterIntegerPref(
      prefs::kUnifiedConsentMigrationState,
      static_cast<int>(MigrationState::kNotInitialized));
#endif
  registry->RegisterBooleanPref(prefs::kPageContentCollectionEnabled, false);
}

void UnifiedConsentService::SetUrlKeyedAnonymizedDataCollectionEnabled(
    bool enabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetMigrationState() != MigrationState::kCompleted)
    SetMigrationState(MigrationState::kCompleted);
#endif

  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            enabled);
}

void UnifiedConsentService::Shutdown() {
  pref_service_->RemoveObserver(this);
  identity_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  if (event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    // By design, clearing the primary account disables URL-keyed data
    // collection.
    SetUrlKeyedAnonymizedDataCollectionEnabled(false);
  }
}

void UnifiedConsentService::OnStateChanged(syncer::SyncService* sync) {
  // Update the UrlKeyedAnonymizedDataCollectionEnabled if user changed the
  // history opt-in state or the explicit-passphrase state.
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    const SyncState new_sync_state = GetSyncState(sync_service_);

    // Before updating the cached state, remember the old value, to detect
    // if anything changed.
    const SyncState old_sync_state = last_sync_state_;
    last_sync_state_ = new_sync_state;

    // Ignore syncing users - UrlKeyedAnonymizedDataCollectionEnabled is
    // updated based on their sync opt-in state.
    if (!sync_service_->HasSyncConsent()) {
      if (ShouldDisableUrlKeyedAnonymizedDataCollection(old_sync_state,
                                                        new_sync_state)) {
        SetUrlKeyedAnonymizedDataCollectionEnabled(false);
      } else if (ShouldEnableUrlKeyedAnonymizedDataCollection(old_sync_state,
                                                              new_sync_state)) {
        SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      }
    }
  }

  // Start observing pref changes when the user enters sync setup.
  // Note: Only |sync->IsSetupInProgress()| is used (i.e. no check for
  // |IsInitialSyncFeatureSetupComplete()|), because on Android
  // |SetInitialSyncFeatureSetupComplete()| is called automatically during the
  // first setup, i.e. the value could change in the meantime.
  // TODO(crbug.com/40067025): Simplify (remove the following block) once
  // kReplaceSyncPromosWithSigninPromos is rolled out on all platforms, and thus
  // IsSetupInProgress() always returns false. See ConsentLevel::kSync
  // documentation for details.
  if (sync->IsSetupInProgress() && !pref_service_->IsSyncing()) {
    StartObservingServicePrefChanges();
  } else {
    StopObservingServicePrefChanges();

    // If the user cancelled the sync setup, clear all observed changes.
    if (!sync->CanSyncFeatureStart())
      service_pref_changes_.clear();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40066949): Simplify (remove the following block) after
  // Sync-the-feature users are migrated to ConsentLevel::kSignin (and thus
  // CanSyncFeatureStart() always returns false).
  if (!sync_service_->CanSyncFeatureStart() ||
      !sync_service_->IsEngineInitialized()) {
    return;
  }

  if (GetMigrationState() == MigrationState::kInProgressWaitForSyncInit)
    UpdateSettingsForMigration();
#endif
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
  const base::Value& value = pref_service_->GetValue(name);
  service_pref_changes_[name] = value.Clone();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
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
  // TODO(crbug.com/40066949): Simplify (remove the following block) after
  // Sync-the-feature users are migrated to ConsentLevel::kSignin, and thus
  // IsSyncFeatureEnabled() always returns false. (The UKM state for kSignin
  // users is set in OnStateChanged(), so no need for the logic here.)
  bool url_keyed_metrics_enabled =
      sync_service_->IsSyncFeatureEnabled() &&
      sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory) &&
      !sync_service_->GetUserSettings()->IsUsingExplicitPassphrase();
  SetUrlKeyedAnonymizedDataCollectionEnabled(url_keyed_metrics_enabled);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  //  namespace unified_consent
