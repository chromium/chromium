// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/ukm_consent_state_observer.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "google_apis/gaia/google_service_auth_error.h"

using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace ukm {

namespace {

bool CanUploadUkmForType(syncer::SyncService* sync_service,
                         syncer::ModelType model_type) {
  switch (GetUploadToGoogleState(sync_service, model_type)) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    // INITIALIZING is considered good enough, because sync is enabled and
    // |model_type| is known to be uploaded to Google, and transient errors
    // don't matter here.
    case syncer::UploadState::INITIALIZING:
    case syncer::UploadState::ACTIVE:
      return true;
  }
}

}  // namespace

UkmConsentStateObserver::UkmConsentStateObserver() = default;

UkmConsentStateObserver::~UkmConsentStateObserver() {
  for (const auto& entry : consent_helpers_) {
    entry.second->RemoveObserver(this);
  }
}

bool UkmConsentStateObserver::ProfileState::AllowsUkm() const {
  return anonymized_data_collection_enabled;
}

// static
UkmConsentStateObserver::ProfileState UkmConsentStateObserver::GetProfileState(
    syncer::SyncService* sync_service,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(sync_service);
  DCHECK(consent_helper);
  ProfileState state;
  state.anonymized_data_collection_enabled = consent_helper->IsEnabled();

  state.extensions_enabled =
      CanUploadUkmForType(sync_service, syncer::ModelType::EXTENSIONS);
  state.apps_sync_enabled =
      CanUploadUkmForType(sync_service, syncer::ModelType::APPS);
  return state;
}

void UkmConsentStateObserver::StartObserving(syncer::SyncService* sync_service,
                                             PrefService* prefs) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper =
      UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(prefs);

  ProfileState state = GetProfileState(sync_service, consent_helper.get());
  previous_states_[sync_service] = state;

  consent_helper->AddObserver(this);
  consent_helpers_[sync_service] = std::move(consent_helper);
  sync_observations_.AddObservation(sync_service);
  UpdateUkmAllowedForAllProfiles(/*total_purge*/ false);
}

void UkmConsentStateObserver::UpdateUkmAllowedForAllProfiles(bool total_purge) {
  const bool previous_states_allow_ukm = CheckPreviousStatesAllowUkm();
  const bool previous_states_allow_apps_ukm =
      previous_states_allow_ukm && CheckPreviousStatesAllowAppsUkm();
  const bool previous_states_allow_extensions_ukm =
      previous_states_allow_ukm && CheckPreviousStatesAllowExtensionUkm();

  UMA_HISTOGRAM_BOOLEAN("UKM.ConsentObserver.AllowedForAllProfiles",
                        previous_states_allow_ukm);

  // Check which consents have changed.
  const bool ukm_consent_changed =
      previous_states_allow_ukm != ukm_allowed_for_all_profiles_;
  const bool apps_consent_changed =
      previous_states_allow_apps_ukm != ukm_allowed_with_apps_for_all_profiles_;
  const bool extension_consent_changed =
      previous_states_allow_extensions_ukm !=
      ukm_allowed_with_extensions_for_all_profiles_;

  // Any change in profile states needs to call OnUkmAllowedStateChanged so that
  // the new settings take effect.
  if (total_purge || ukm_consent_changed || extension_consent_changed ||
      apps_consent_changed) {
    ukm_allowed_for_all_profiles_ = previous_states_allow_ukm;
    ukm_allowed_with_apps_for_all_profiles_ = previous_states_allow_apps_ukm;
    ukm_allowed_with_extensions_for_all_profiles_ =
        previous_states_allow_extensions_ukm;

    OnUkmAllowedStateChanged(total_purge);
  }
}

bool UkmConsentStateObserver::CheckPreviousStatesAllowUkm() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const ProfileState& state = kv.second;
    if (!state.AllowsUkm())
      return false;
  }

  return true;
}

bool UkmConsentStateObserver::CheckPreviousStatesAllowAppsUkm() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const ProfileState& state = kv.second;
    if (!state.apps_sync_enabled)
      return false;
  }
  return true;
}

bool UkmConsentStateObserver::CheckPreviousStatesAllowExtensionUkm() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const ProfileState& state = kv.second;
    if (!state.extensions_enabled)
      return false;
  }
  return true;
}

void UkmConsentStateObserver::OnStateChanged(syncer::SyncService* sync) {
  UrlKeyedDataCollectionConsentHelper* consent_helper = nullptr;
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end())
    consent_helper = found->second.get();
  UpdateProfileState(sync, consent_helper);
}

void UkmConsentStateObserver::OnUrlKeyedDataCollectionConsentStateChanged(
    unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(consent_helper);
  syncer::SyncService* sync_service = nullptr;
  for (const auto& entry : consent_helpers_) {
    if (consent_helper == entry.second.get()) {
      sync_service = entry.first;
      break;
    }
  }
  DCHECK(sync_service);
  UpdateProfileState(sync_service, consent_helper);
}

void UkmConsentStateObserver::UpdateProfileState(
    syncer::SyncService* sync,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(base::Contains(previous_states_, sync));
  const ProfileState& previous_state = previous_states_[sync];
  DCHECK(consent_helper);
  ProfileState state = GetProfileState(sync, consent_helper);

  // Trigger a total purge of all local UKM data if the current state no longer
  // allows tracking UKM.
  bool total_purge = previous_state.AllowsUkm() && !state.AllowsUkm();

  UMA_HISTOGRAM_BOOLEAN("UKM.ConsentObserver.Purge", total_purge);

  previous_states_[sync] = state;
  UpdateUkmAllowedForAllProfiles(total_purge);
}

void UkmConsentStateObserver::OnSyncShutdown(syncer::SyncService* sync) {
  DCHECK(base::Contains(previous_states_, sync));
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end()) {
    found->second->RemoveObserver(this);
    consent_helpers_.erase(found);
  }
  DCHECK(sync_observations_.IsObservingSource(sync));
  sync_observations_.RemoveObservation(sync);
  previous_states_.erase(sync);
  UpdateUkmAllowedForAllProfiles(/*total_purge=*/false);
}

bool UkmConsentStateObserver::IsUkmAllowedForAllProfiles() {
  return ukm_allowed_for_all_profiles_;
}

bool UkmConsentStateObserver::IsUkmAllowedWithAppsForAllProfiles() {
  return ukm_allowed_with_apps_for_all_profiles_;
}

bool UkmConsentStateObserver::IsUkmAllowedWithExtensionsForAllProfiles() {
  return ukm_allowed_with_extensions_for_all_profiles_;
}

}  // namespace ukm
