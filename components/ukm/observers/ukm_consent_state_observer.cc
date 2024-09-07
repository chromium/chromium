// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/ukm_consent_state_observer.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/metrics/metrics_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "google_apis/gaia/google_service_auth_error.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"  // nogncheck
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace ukm {
namespace {

bool IsMsbbConsentStateAllowed() {
#if BUILDFLAG(IS_CHROMEOS)
  // MGS should report only AppKM metrics.
  return !chromeos::IsManagedGuestSession();
#else  // !BUILDFLAG(IS_CHROMEOS)
  return true;
#endif
}

bool CanUploadUkmForType(syncer::SyncService* sync_service,
                         syncer::DataType data_type,
                         bool msbb_consent) {
#if BUILDFLAG(IS_CHROMEOS)
  // Enable uploading of UKM for Kiosk and MGS only if MSBB consent is set.
  if (chromeos::IsKioskSession() || chromeos::IsManagedGuestSession()) {
    return msbb_consent;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  switch (GetUploadToGoogleState(sync_service, data_type)) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    // INITIALIZING is considered good enough, because sync is enabled and
    // |data_type| is known to be uploaded to Google, and transient errors
    // don't matter here.
    case syncer::UploadState::INITIALIZING:
    case syncer::UploadState::ACTIVE:
      return true;
  }
}
}  // namespace

UkmConsentStateObserver::UkmConsentStateObserver()
    : ukm_consent_state_(UkmConsentState()) {}

UkmConsentStateObserver::UkmConsentStateObserver(NoInitialUkmConsentStateTag)
    : ukm_consent_state_(std::nullopt) {}

UkmConsentStateObserver::~UkmConsentStateObserver() {
  for (const auto& entry : consent_helpers_) {
    entry.second->RemoveObserver(this);
  }
}

bool UkmConsentStateObserver::ProfileState::IsUkmConsented() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return consent_state.Has(MSBB) || consent_state.Has(APPS);
#else
  return consent_state.Has(MSBB);
#endif
}

void UkmConsentStateObserver::ProfileState::SetConsentType(
    UkmConsentType type) {
  consent_state.Put(type);
}

UkmConsentStateObserver::ProfileState UkmConsentStateObserver::GetProfileState(
    syncer::SyncService* sync_service,
    UrlKeyedDataCollectionConsentHelper* consent_helper) {
  DCHECK(sync_service);
  DCHECK(consent_helper);
  ProfileState state;

  const bool msbb_consent =
      consent_helper->IsEnabled() || metrics::IsMsbbSettingForcedOnForUkm();

  if (msbb_consent && IsMsbbConsentStateAllowed()) {
    state.SetConsentType(MSBB);
  }

  if (msbb_consent &&
      CanUploadUkmForType(sync_service, syncer::DataType::EXTENSIONS,
                          msbb_consent)) {
    state.SetConsentType(EXTENSIONS);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool app_sync_consent =
      CanUploadUkmForType(sync_service, syncer::DataType::APPS, msbb_consent) ||
      // Demo mode is a special managed guest session that doesn't support
      // AppKM. To support AppKM an exception needs to be made within UKM.
      IsDeviceInDemoMode();

  if (app_sync_consent) {
    state.SetConsentType(APPS);
  }
#else
  // This separation isn't actually needed for non-ChromeOS devices. But for
  // clarity it is added.
  if (msbb_consent &&
      CanUploadUkmForType(sync_service, syncer::DataType::APPS, msbb_consent)) {
    state.SetConsentType(APPS);
  }
#endif

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
  const UkmConsentState new_state = GetPreviousStatesForAllProfiles();

  // Any change in profile states needs to call OnUkmAllowedStateChanged so that
  // the new settings take effect. If ukm_consent_state_ is std::nullopt (i.e.
  // no profile loaded yet), this will always be considered as a change.
  if (total_purge || new_state != ukm_consent_state_) {
    if (ukm_consent_state_.has_value()) {
      // Records whether the App sync consent changed when the consent state is
      // updated. This is to see how often App sync is changed by users.
      base::UmaHistogramBoolean(
          "UKM.ConsentObserver.AppSyncConsentChanged",
          ukm_consent_state_->Has(APPS) != new_state.Has(APPS));
    }

    const auto previous_consent_state = GetUkmConsentState();
    ukm_consent_state_ = new_state;
    OnUkmAllowedStateChanged(total_purge, previous_consent_state);
  }
}

UkmConsentState UkmConsentStateObserver::GetPreviousStatesForAllProfiles() {
  // No profiles are being observed, no consent is possible.
  if (previous_states_.empty()) {
    return UkmConsentState();
  }

  // Consent for each type must be given by all profiles for metrics of that
  // type to be collected. See components/ukm/ukm_consent_state.h for details.
  // Performs an AND over all of the profiles' consents states in
  // |profile_states_|. Must assume all consent types are granted for the
  // AND operation to work as expected.
  auto state = UkmConsentState::All();
  for (const auto& kv : previous_states_) {
    const ProfileState& profile = kv.second;
    state = base::Intersection(state, profile.consent_state);
  }

  return state;
}

void UkmConsentStateObserver::OnStateChanged(syncer::SyncService* sync) {
  UrlKeyedDataCollectionConsentHelper* consent_helper = nullptr;
  auto found = consent_helpers_.find(sync);
  if (found != consent_helpers_.end()) {
    consent_helper = found->second.get();
  }
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
  // allows recording of UKMs.
  bool total_purge = previous_state.IsUkmConsented() && !state.IsUkmConsented();

  base::UmaHistogramBoolean("UKM.ConsentObserver.Purge", total_purge);

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
  const UkmConsentState ukm_consent_state = GetUkmConsentState();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ukm_consent_state.Has(MSBB) || ukm_consent_state.Has(APPS);
#else
  return ukm_consent_state.Has(MSBB);
#endif
}

UkmConsentState UkmConsentStateObserver::GetUkmConsentState() {
  // Consider that the state is empty if it is std::nullopt (the not set
  // state is only relevant to detect when the first profile is loaded).
  return ukm_consent_state_.value_or(UkmConsentState());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void UkmConsentStateObserver::SetIsDemoMode(bool is_device_in_demo_mode) {
  is_device_in_demo_mode_ = is_device_in_demo_mode;
}

bool UkmConsentStateObserver::IsDeviceInDemoMode() {
  return is_device_in_demo_mode_;
}
#endif

}  // namespace ukm
