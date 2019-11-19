// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/ukm_consent_state_observer.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "google_apis/gaia/google_service_auth_error.h"

using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace ukm {

UkmConsentStateObserver::UkmConsentStateObserver() : sync_observer_(this) {}

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
      sync_service->GetPreferredDataTypes().Has(syncer::EXTENSIONS) &&
      sync_service->IsSyncFeatureEnabled();
  return state;
}

void UkmConsentStateObserver::StartObserving(syncer::SyncService* sync_service,
                                             PrefService* prefs) {
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper> consent_helper =
      UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(prefs, sync_service);

  ProfileState state = GetProfileState(sync_service, consent_helper.get());
  previous_states_[sync_service] = state;

  consent_helper->AddObserver(this);
  consent_helpers_[sync_service] = std::move(consent_helper);
  sync_observer_.Add(sync_service);
  UpdateUkmAllowedForAllProfiles(false);
}

void UkmConsentStateObserver::UpdateUkmAllowedForAllProfiles(bool must_purge) {
  bool all_profile_states_allow_ukm = CheckPreviousStatesAllowUkm();
  bool all_profile_states_allow_extension_ukm =
      all_profile_states_allow_ukm && CheckPreviousStatesAllowExtensionUkm();

  UMA_HISTOGRAM_BOOLEAN("UKM.ConsentObserver.AllowedForAllProfiles",
                        all_profile_states_allow_ukm);

  // Any change in profile states needs to call OnUkmAllowedStateChanged so that
  // the new settings take effect.
  if (must_purge ||
      (all_profile_states_allow_ukm != ukm_allowed_for_all_profiles_) ||
      (all_profile_states_allow_extension_ukm !=
       ukm_allowed_with_extensions_for_all_profiles_)) {
    ukm_allowed_for_all_profiles_ = all_profile_states_allow_ukm;
    ukm_allowed_with_extensions_for_all_profiles_ =
        all_profile_states_allow_extension_ukm;
    OnUkmAllowedStateChanged(must_purge);
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

  // Trigger a purge if the current state no longer allows UKM.
  bool must_purge = previous_state.AllowsUkm() && !state.AllowsUkm();

  UMA_HISTOGRAM_BOOLEAN("UKM.ConsentObserver.Purge", must_purge);

  previous_states_[sync] = state;
  UpdateUkmAllowedForAllProfiles(must_purge);
}

void UkmConsentStateObserver::OnSyncShutdown(syncer::SyncService* sync) {
  DCHECK(base::Contains(previous_states_, sync));
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end()) {
    found->second->RemoveObserver(this);
    consent_helpers_.erase(found);
  }
  sync_observer_.Remove(sync);
  previous_states_.erase(sync);
  UpdateUkmAllowedForAllProfiles(/*must_purge=*/false);
}

bool UkmConsentStateObserver::IsUkmAllowedForAllProfiles() {
  return ukm_allowed_for_all_profiles_;
}

bool UkmConsentStateObserver::IsUkmAllowedWithExtensionsForAllProfiles() {
  return ukm_allowed_with_extensions_for_all_profiles_;
}

}  // namespace ukm
