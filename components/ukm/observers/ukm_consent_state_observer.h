// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_OBSERVERS_UKM_CONSENT_STATE_OBSERVER_H_
#define COMPONENTS_UKM_OBSERVERS_UKM_CONSENT_STATE_OBSERVER_H_

#include <map>

#include "base/scoped_multi_source_observation.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

class PrefService;

namespace ukm {

// Observer that monitors whether UKM is allowed for all profiles.
//
// For one profile, UKM is allowed iff URL-keyed anonymized data collection is
// enabled.
class UkmConsentStateObserver
    : public syncer::SyncServiceObserver,
      public unified_consent::UrlKeyedDataCollectionConsentHelper::Observer {
 public:
  UkmConsentStateObserver();

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

  // Returns true iff sync is in a state that allows UKM to capture apps.
  // This means that all profiles have APPS data type enabled for syncing.
  virtual bool IsUkmAllowedWithAppsForAllProfiles();

  // Returns true iff sync is in a state that allows UKM to capture extensions.
  // This means that all profiles have EXTENSIONS data type enabled for syncing.
  virtual bool IsUkmAllowedWithExtensionsForAllProfiles();

 protected:
  // Called after UKM consent state changed.
  // If |total_purge| is true, the UKM is not allowed for some profile, and all
  // local data must be purged. Otherwise, more specific consents are checked
  // for individual sync settings, and recorded data may be partially purged if
  // we no longer have the corresponding sync consent.
  virtual void OnUkmAllowedStateChanged(bool total_purge) = 0;

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

  // Returns true iff all profile states in |previous_states_| allow UKM.
  // If there are no profiles being observed, this returns false.
  bool CheckPreviousStatesAllowUkm();

  // Returns true iff all profile states in |previous_states_| allow extension
  // UKM. If no profiles are being observed, this returns false.
  bool CheckPreviousStatesAllowExtensionUkm();

  // Returns true iff all profile states in |previous_states_| allow apps
  // UKM. If no profiles are being observed, this returns false.
  bool CheckPreviousStatesAllowAppsUkm();

  // Tracks observed sync services, for cleanup.
  base::ScopedMultiSourceObservation<syncer::SyncService,
                                     syncer::SyncServiceObserver>
      sync_observations_{this};

  // State data about profiles that we need to remember.
  struct ProfileState {
    // Returns true if this state allows UKM (i.e. URL-keyed anonymized
    // data collection is enabled).
    bool AllowsUkm() const;

    // Whether anonymized data collection is enabled.
    bool anonymized_data_collection_enabled = false;

    // Whether the user has extension sync enabled.
    bool extensions_enabled = false;

    // Whether the user has apps sync enabled.
    bool apps_sync_enabled = false;
  };

  // Updates the UKM enabled state for a profile and then triggers an update of
  // the state for all profiles.
  // |sync| and |consent_helper| must not be null.
  void UpdateProfileState(
      syncer::SyncService* sync,
      unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper);

  // Gets the current state of a profile.
  // |sync| and |consent_helper| must not be null.
  static ProfileState GetProfileState(
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

  // Tracks if UKM is allowed on all profiles after the last state change.
  bool ukm_allowed_for_all_profiles_ = false;

  // Tracks if apps sync was enabled on all profiles after the last state
  // change.
  bool ukm_allowed_with_apps_for_all_profiles_ = false;

  // Tracks if extension sync was enabled on all profiles after the last state
  // change.
  bool ukm_allowed_with_extensions_for_all_profiles_ = false;
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_OBSERVERS_UKM_CONSENT_STATE_OBSERVER_H_
