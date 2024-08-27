// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_OBSERVERS_UKM_CONSENT_STATE_OBSERVER_H_
#define COMPONENTS_UKM_OBSERVERS_UKM_CONSENT_STATE_OBSERVER_H_

#include <stdint.h>

#include <map>
#include <optional>

#include "base/feature_list.h"
#include "base/scoped_multi_source_observation.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/ukm/ukm_consent_state.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "services/metrics/public/cpp/metrics_export.h"

class PrefService;

namespace ukm {

// Marker type used to indicate that the initial UkmConsentState should
// be left in an uninitialized state (i.e. std::nullopt).
struct NoInitialUkmConsentStateTag {};
constexpr NoInitialUkmConsentStateTag NoInitialUkmConsentState;

// Observer that monitors whether UKM is allowed for all profiles.
//
// For one profile, UKM is allowed iff URL-keyed anonymized data collection is
// enabled.
class UkmConsentStateObserver
    : public syncer::SyncServiceObserver,
      public unified_consent::UrlKeyedDataCollectionConsentHelper::Observer {
 public:
  UkmConsentStateObserver();
  UkmConsentStateObserver(NoInitialUkmConsentStateTag);

  UkmConsentStateObserver(const UkmConsentStateObserver&) = delete;
  UkmConsentStateObserver& operator=(const UkmConsentStateObserver&) = delete;

  ~UkmConsentStateObserver() override;

  // Starts observing whether UKM is allowed for a profile.
  // |pref_service| is the pref service of a profile.
  void StartObserving(syncer::SyncService* sync_service,
                      PrefService* pref_service);

  // Returns true iff all UKM is allowed for all profile states. This means that
  // URL-keyed anonymized data collection is enabled for all profiles.
  virtual bool IsUkmAllowedForAllProfiles();

  // Returns the current state of all consent types.
  // See components/ukm/ukm_consent_state.h for details.
  virtual UkmConsentState GetUkmConsentState();

 protected:
  // Called after UKM consent state changed.
  // If |total_purge| is true, the UKM is not allowed for some profile, and all
  // local data must be purged. Otherwise, more specific consents are checked
  // for individual sync settings, and recorded data may be partially purged if
  // we no longer have the corresponding sync consent.
  virtual void OnUkmAllowedStateChanged(
      bool total_purge,
      UkmConsentState previous_consent_state) = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Used to set is_demo_mode_ field.
  void SetIsDemoMode(bool is_demo_mode);

  // Return whether the device is in demo mode.
  bool IsDeviceInDemoMode();
#endif

 private:
  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // unified_consent::UrlKeyedDataCollectionConsentHelper::Observer:
  void OnUrlKeyedDataCollectionConsentStateChanged(
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper)
      override;

  // Recomputes |ukm_allowed_for_all_profiles_| and other specific consents
  // (e.g. Extension, ChromeOS apps) from |previous_states_|;
  void UpdateUkmAllowedForAllProfiles(bool total_purge);

  // Returns an accumulated UKM consent state for all profiles in
  // |previous_states_|. Consent is given for any consent type in
  // UkmConsentType IFF all profiles consent, otherwise the consent
  // will be off.
  UkmConsentState GetPreviousStatesForAllProfiles();

  // Tracks observed sync services, for cleanup.
  base::ScopedMultiSourceObservation<syncer::SyncService,
                                     syncer::SyncServiceObserver>
      sync_observations_{this};

  // State data about profiles that we need to remember.
  struct ProfileState {
    // Returns true if this profile state consented to MSBB (i.e. URL-keyed
    // anonymized data collection is enabled).
    // False otherwise.
    bool IsUkmConsented() const;

    // Set the consent state for the given type.
    void SetConsentType(UkmConsentType type);

    // The state of each consent type.
    UkmConsentState consent_state;
  };

  // Updates the UKM enabled state for a profile and then triggers an update of
  // the state for all profiles.
  // |sync| and |consent_helper| must not be null.
  void UpdateProfileState(
      syncer::SyncService* sync,
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper);

  // Gets the current state of a profile.
  // |sync| and |consent_helper| must not be null.
  ProfileState GetProfileState(
      syncer::SyncService* sync,
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper);

  // The state of the profile being observed.
  //
  // Note: UKM consent does not rely on sync but there must be exactly one
  // sync service per profile, so it is safe to key the profile states by the
  // sync service.
  std::map<syncer::SyncService*, ProfileState> previous_states_;

  // The list of URL-keyed anonymized data collection consent helpers.
  //
  // Note: UrlKeyedDataCollectionConsentHelper does not rely on sync but there
  // must be exactly one per Chromium profile. As there is a single sync service
  // per profile, it is safe to key them by sync service instead of introducing
  // an additional map.
  std::map<
      syncer::SyncService*,
      std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>>
      consent_helpers_;

  // Tracks what consent type is granted on all profiles after the last state
  // change. Consent is only granted when EVERY profile consents.
  // Empty means none.
  //
  // std::nullopt means that no profile has been loaded yet. This is only used
  // if constructed with UkmConsentStateObserver(NoInitialUkmConsentStateTag).
  std::optional<UkmConsentState> ukm_consent_state_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Indicate whether the device is in demo mode. If it is true,
  // set APPS consent to collect App usage data for active demo
  // session. Default to false.
  bool is_device_in_demo_mode_ = false;
#endif
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_OBSERVERS_UKM_CONSENT_STATE_OBSERVER_H_
