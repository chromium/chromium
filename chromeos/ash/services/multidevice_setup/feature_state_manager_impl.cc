// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/feature_state_manager_impl.h"

#include <array>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/multidevice_setup/global_state_feature_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace multidevice_setup {

namespace {

constexpr base::TimeDelta kFeatureStateLoggingPeriod = base::Minutes(30);

constexpr std::array<mojom::Feature, 4> kPhoneHubSubFeatures{
    mojom::Feature::kPhoneHubNotifications, mojom::Feature::kPhoneHubCameraRoll,
    mojom::Feature::kPhoneHubTaskContinuation, mojom::Feature::kEche};

base::flat_map<mojom::Feature, std::string>
GenerateFeatureToEnabledPrefNameMap() {
  return base::flat_map<mojom::Feature, std::string>{
      {mojom::Feature::kBetterTogetherSuite,
       kBetterTogetherSuiteEnabledPrefName},
      {mojom::Feature::kInstantTethering, kInstantTetheringEnabledPrefName},
      {mojom::Feature::kSmartLock, kSmartLockEnabledPrefName},
      {mojom::Feature::kPhoneHub, kPhoneHubEnabledPrefName},
      {mojom::Feature::kPhoneHubCameraRoll, kPhoneHubCameraRollEnabledPrefName},
      {mojom::Feature::kPhoneHubNotifications,
       kPhoneHubNotificationsEnabledPrefName},
      {mojom::Feature::kPhoneHubTaskContinuation,
       kPhoneHubTaskContinuationEnabledPrefName},
      {mojom::Feature::kEche, kEcheEnabledPrefName}};
}

base::flat_map<mojom::Feature, std::string>
GenerateFeatureToAllowedPrefNameMap() {
  return base::flat_map<mojom::Feature, std::string>{
      {mojom::Feature::kInstantTethering, kInstantTetheringAllowedPrefName},
      {mojom::Feature::kSmartLock, kSmartLockAllowedPrefName},
      {mojom::Feature::kPhoneHub, kPhoneHubAllowedPrefName},
      {mojom::Feature::kPhoneHubCameraRoll, kPhoneHubCameraRollAllowedPrefName},
      {mojom::Feature::kPhoneHubNotifications,
       kPhoneHubNotificationsAllowedPrefName},
      {mojom::Feature::kPhoneHubTaskContinuation,
       kPhoneHubTaskContinuationAllowedPrefName},
      {mojom::Feature::kWifiSync, kWifiSyncAllowedPrefName},
      {mojom::Feature::kEche, kEcheAllowedPrefName}};
}

// Each feature's default value is kUnavailableNoVerifiedHost_NoEligibleHosts
// until proven otherwise.
base::flat_map<mojom::Feature, mojom::FeatureState>
GenerateInitialDefaultCachedStateMap() {
  return base::flat_map<mojom::Feature, mojom::FeatureState>{
      {mojom::Feature::kBetterTogetherSuite,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kInstantTethering,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kSmartLock,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kPhoneHub,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kPhoneHubCameraRoll,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kPhoneHubNotifications,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kPhoneHubTaskContinuation,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kWifiSync,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kEche,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
  };
}

void ProcessSuiteEdgeCases(
    FeatureStateManager::FeatureStatesMap* feature_states_map_ptr) {
  FeatureStateManager::FeatureStatesMap& feature_states_map =
      *feature_states_map_ptr;

  // If the top-level Phone Hub feature is prohibited by policy, all of the
  // sub-features are implicitly prohibited as well.
  if (feature_states_map[mojom::Feature::kPhoneHub] ==
      mojom::FeatureState::kProhibitedByPolicy) {
    for (const auto& phone_hub_sub_feature : kPhoneHubSubFeatures) {
      feature_states_map[phone_hub_sub_feature] =
          mojom::FeatureState::kProhibitedByPolicy;
    }
  }

  bool are_all_sub_features_prohibited_or_unsupported = true;
  bool is_at_least_one_feature_supported = false;
  for (const auto& map_entry : feature_states_map) {
    // Skip the suite feature, since it doesn't have its own policy.
    if (map_entry.first == mojom::Feature::kBetterTogetherSuite) {
      continue;
    }

    const mojom::FeatureState feature_state = map_entry.second;

    if (feature_state != mojom::FeatureState::kNotSupportedByChromebook) {
      is_at_least_one_feature_supported = true;
    }

    // Also check for features that are not supported by the Chromebook, since
    // we should still consider the suite prohibited if all sub-features are
    // prohibited except for those that aren't even shown in the UI at all.
    if (feature_state != mojom::FeatureState::kProhibitedByPolicy &&
        feature_state != mojom::FeatureState::kNotSupportedByChromebook) {
      are_all_sub_features_prohibited_or_unsupported = false;
    }
  }

  // If no features are supported, the suite as a whole is considered
  // unsupported.
  if (!is_at_least_one_feature_supported) {
    feature_states_map[mojom::Feature::kBetterTogetherSuite] =
        mojom::FeatureState::kNotSupportedByChromebook;
    return;
  }

  // The Better Together suite does not have its own explicit device policy;
  // instead, if all supported sub-features are prohibited by policy, the entire
  // suite should be considered prohibited.
  if (are_all_sub_features_prohibited_or_unsupported) {
    feature_states_map[mojom::Feature::kBetterTogetherSuite] =
        mojom::FeatureState::kProhibitedByPolicy;
    return;
  }

  // If the Better Together suite is disabled by the user, any sub-features
  // which have been enabled by the user should be unavailable. The suite serves
  // as a gatekeeper to all sub-features.
  if (feature_states_map[mojom::Feature::kBetterTogetherSuite] ==
      mojom::FeatureState::kDisabledByUser) {
    for (auto& map_entry : feature_states_map) {
      mojom::FeatureState& feature_state = map_entry.second;
      if (feature_state == mojom::FeatureState::kEnabledByUser) {
        feature_state = mojom::FeatureState::kUnavailableSuiteDisabled;
      }
    }
  }

  // If the top-level Phone Hub feature is disabled, its sub-features are
  // unavailable.
  if (feature_states_map[mojom::Feature::kPhoneHub] ==
      mojom::FeatureState::kDisabledByUser) {
    for (const auto& phone_hub_sub_feature : kPhoneHubSubFeatures) {
      mojom::FeatureState& feature_state =
          feature_states_map[phone_hub_sub_feature];
      if (feature_state == mojom::FeatureState::kEnabledByUser ||
          feature_state == mojom::FeatureState::kUnavailableSuiteDisabled) {
        feature_state =
            mojom::FeatureState::kUnavailableTopLevelFeatureDisabled;
      }
    }
  }

  // If the top level Phone Hub feature is not supported by the phone, the
  // sub-features should also be not supported by the phone.
  if (feature_states_map[mojom::Feature::kPhoneHub] ==
      mojom::FeatureState::kNotSupportedByPhone) {
    for (const auto& phone_hub_sub_feature : kPhoneHubSubFeatures) {
      feature_states_map[phone_hub_sub_feature] =
          mojom::FeatureState::kNotSupportedByPhone;
    }
  }
}

}  // namespace

// static
FeatureStateManagerImpl::Factory*
    FeatureStateManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<FeatureStateManager> FeatureStateManagerImpl::Factory::Create(
    PrefService* pref_service,
    HostStatusProvider* host_status_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
    const base::flat_map<mojom::Feature,
                         raw_ptr<GlobalStateFeatureManager, CtnExperimental>>&
        global_state_feature_managers,
    bool is_secondary_user) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        pref_service, host_status_provider, device_sync_client,
        android_sms_pairing_state_tracker, global_state_feature_managers,
        is_secondary_user);
  }

  return base::WrapUnique(new FeatureStateManagerImpl(
      pref_service, host_status_provider, device_sync_client,
      android_sms_pairing_state_tracker, global_state_feature_managers,
      is_secondary_user));
}

// static
void FeatureStateManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

FeatureStateManagerImpl::Factory::~Factory() = default;

FeatureStateManagerImpl::FeatureStateManagerImpl(
    PrefService* pref_service,
    HostStatusProvider* host_status_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker,
    const base::flat_map<mojom::Feature,
                         raw_ptr<GlobalStateFeatureManager, CtnExperimental>>&
        global_state_feature_managers,
    bool is_secondary_user)
    : pref_service_(pref_service),
      host_status_provider_(host_status_provider),
      device_sync_client_(device_sync_client),
      android_sms_pairing_state_tracker_(android_sms_pairing_state_tracker),
      global_state_feature_managers_(global_state_feature_managers),
      is_secondary_user_(is_secondary_user),
      feature_to_enabled_pref_name_map_(GenerateFeatureToEnabledPrefNameMap()),
      feature_to_allowed_pref_name_map_(GenerateFeatureToAllowedPrefNameMap()),
      cached_feature_state_map_(GenerateInitialDefaultCachedStateMap()) {
  host_status_provider_->AddObserver(this);
  device_sync_client_->AddObserver(this);
  if (android_sms_pairing_state_tracker_) {
    android_sms_pairing_state_tracker_->AddObserver(this);
  }

  registrar_.Init(pref_service_);

  // Listen for changes to each of the "enabled" feature names.
  for (const auto& map_entry : feature_to_enabled_pref_name_map_) {
    registrar_.Add(
        map_entry.second,
        base::BindRepeating(&FeatureStateManagerImpl::OnPrefValueChanged,
                            base::Unretained(this)));
  }

  // Also listen for changes to each of the "allowed" feature names.
  for (const auto& map_entry : feature_to_allowed_pref_name_map_) {
    registrar_.Add(
        map_entry.second,
        base::BindRepeating(&FeatureStateManagerImpl::OnPrefValueChanged,
                            base::Unretained(this)));
  }

  registrar_.Add(
      kEcheOverriddenSupportReceivedFromPhoneHubPrefName,
      base::BindRepeating(&FeatureStateManagerImpl::OnPrefValueChanged,
                          base::Unretained(this)));

  // Prime the cache. Since this is the initial computation, it does not
  // represent a true change of feature state values, so observers should not be
  // notified.
  UpdateFeatureStateCache(false /* notify_observers_of_changes */);

  LogFeatureStates();
  feature_state_metric_timer_.Start(
      FROM_HERE, kFeatureStateLoggingPeriod,
      base::BindRepeating(&FeatureStateManagerImpl::LogFeatureStates,
                          base::Unretained(this)));
}

FeatureStateManagerImpl::~FeatureStateManagerImpl() {
  host_status_provider_->RemoveObserver(this);
  device_sync_client_->RemoveObserver(this);
  if (android_sms_pairing_state_tracker_) {
    android_sms_pairing_state_tracker_->RemoveObserver(this);
  }
}

FeatureStateManager::FeatureStatesMap
FeatureStateManagerImpl::GetFeatureStates() {
  return cached_feature_state_map_;
}

void FeatureStateManagerImpl::PerformSetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled) {
  if (global_state_feature_managers_.contains(feature)) {
    global_state_feature_managers_.at(feature)->SetIsFeatureEnabled(enabled);
    // Need to manually trigger UpdateFeatureStateCache since changes to
    // this global feature state is not observed by |registrar_| and will not
    // invoke OnPrefValueChanged
    UpdateFeatureStateCache(true /* notify_observers_of_changes */);
    return;
  }

  // Note: Since |registrar_| observes changes to all relevant preferences,
  // this call will result in OnPrefValueChanged() being invoked, resulting in
  // observers being notified of the change.
  pref_service_->SetBoolean(feature_to_enabled_pref_name_map_[feature],
                            enabled);
}

void FeatureStateManagerImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  UpdateFeatureStateCache(true /* notify_observers_of_changes */);
}

void FeatureStateManagerImpl::OnNewDevicesSynced() {
  UpdateFeatureStateCache(true /* notify_observers_of_changes */);
}

void FeatureStateManagerImpl::OnPrefValueChanged() {
  UpdateFeatureStateCache(true /* notify_observers_of_changes */);
}

void FeatureStateManagerImpl::OnPairingStateChanged() {
  UpdateFeatureStateCache(true /* notify_observers_of_changes */);
}

void FeatureStateManagerImpl::UpdateFeatureStateCache(
    bool notify_observers_of_changes) {
  // Make a copy of |cached_feature_state_map_| before making edits to it.
  FeatureStatesMap previous_cached_feature_state_map =
      cached_feature_state_map_;

  // Update |cached_feature_state_map_| with computed values.
  auto it = cached_feature_state_map_.begin();
  while (it != cached_feature_state_map_.end()) {
    it->second = ComputeFeatureState(it->first);
    ++it;
  }

  // Some computed values must be updated to support various edge cases.
  ProcessSuiteEdgeCases(&cached_feature_state_map_);

  if (previous_cached_feature_state_map == cached_feature_state_map_) {
    return;
  }
  PA_LOG(INFO) << "Feature states map changed. Old map: "
               << previous_cached_feature_state_map
               << ", new map: " << cached_feature_state_map_;
  LogFeatureStates();
  NotifyFeatureStatesChange(cached_feature_state_map_);
}

mojom::FeatureState FeatureStateManagerImpl::ComputeFeatureState(
    mojom::Feature feature) {
  if (!IsAllowedByPolicy(feature)) {
    return mojom::FeatureState::kProhibitedByPolicy;
  }

  HostStatusProvider::HostStatusWithDevice status_with_device =
      host_status_provider_->GetHostWithStatus();

  if (status_with_device.host_status() == mojom::HostStatus::kNoEligibleHosts) {
    return mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts;
  }

  if (status_with_device.host_status() != mojom::HostStatus::kHostVerified) {
    return mojom::FeatureState::
        kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified;
  }

  if (!IsSupportedByChromebook(feature)) {
    return mojom::FeatureState::kNotSupportedByChromebook;
  }

  if (!HasSufficientSecurity(feature, *status_with_device.host_device())) {
    return mojom::FeatureState::kUnavailableInsufficientSecurity;
  }

  if (!HasBeenActivatedByPhone(feature, *status_with_device.host_device())) {
    return mojom::FeatureState::kNotSupportedByPhone;
  }

  return GetEnabledOrDisabledState(feature);
}

bool FeatureStateManagerImpl::IsAllowedByPolicy(mojom::Feature feature) {
  // If no policy preference exists for this feature, the feature is implicitly
  // allowed.
  if (!base::Contains(feature_to_allowed_pref_name_map_, feature)) {
    return true;
  }

  return pref_service_->GetBoolean(feature_to_allowed_pref_name_map_[feature]);
}

bool FeatureStateManagerImpl::IsSupportedByChromebook(mojom::Feature feature) {
  if (!features::IsCrossDeviceFeatureSuiteAllowed()) {
    return false;
  }

  static const std::pair<mojom::Feature, multidevice::SoftwareFeature>
      kFeatureAndClientSoftwareFeaturePairs[] = {
          {mojom::Feature::kBetterTogetherSuite,
           multidevice::SoftwareFeature::kBetterTogetherClient},
          {mojom::Feature::kInstantTethering,
           multidevice::SoftwareFeature::kInstantTetheringClient},
          {mojom::Feature::kSmartLock,
           multidevice::SoftwareFeature::kSmartLockClient},
          // Note: Most Phone Hub-related features use the same SoftwareFeature.
          {mojom::Feature::kPhoneHub,
           multidevice::SoftwareFeature::kPhoneHubClient},
          {mojom::Feature::kPhoneHubNotifications,
           multidevice::SoftwareFeature::kPhoneHubClient},
          {mojom::Feature::kPhoneHubTaskContinuation,
           multidevice::SoftwareFeature::kPhoneHubClient},
          // Note: Camera Roll is launched separately from the rest of PhoneHub.
          {mojom::Feature::kPhoneHubCameraRoll,
           multidevice::SoftwareFeature::kPhoneHubCameraRollClient},
          {mojom::Feature::kWifiSync,
           multidevice::SoftwareFeature::kWifiSyncClient},
          {mojom::Feature::kEche, multidevice::SoftwareFeature::kEcheClient}};

  std::optional<multidevice::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();
  if (!local_device) {
    PA_LOG(ERROR) << "FeatureStateManagerImpl::" << __func__
                  << ": Local device unexpectedly null.";
    return false;
  }

  for (const auto& pair : kFeatureAndClientSoftwareFeaturePairs) {
    if (pair.first != feature) {
      continue;
    }

    if ((pair.second == multidevice::SoftwareFeature::kPhoneHubClient ||
         pair.second == multidevice::SoftwareFeature::kEcheClient) &&
        is_secondary_user_) {
      return false;
    }

    return local_device->GetSoftwareFeatureState(pair.second) !=
           multidevice::SoftwareFeatureState::kNotSupported;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool FeatureStateManagerImpl::HasSufficientSecurity(
    mojom::Feature feature,
    const multidevice::RemoteDeviceRef& host_device) {
  if (feature != mojom::Feature::kSmartLock) {
    return true;
  }

  // Special case for Smart Lock: if the host device does not have a lock screen
  // set, its SoftwareFeatureState for kSmartLockHost is supported but not
  // enabled.
  return host_device.GetSoftwareFeatureState(
             multidevice::SoftwareFeature::kSmartLockHost) !=
         multidevice::SoftwareFeatureState::kSupported;
}

bool FeatureStateManagerImpl::HasBeenActivatedByPhone(
    mojom::Feature feature,
    const multidevice::RemoteDeviceRef& host_device) {
  static const std::pair<mojom::Feature, multidevice::SoftwareFeature>
      kFeatureAndHostSoftwareFeaturePairs[] = {
          {mojom::Feature::kBetterTogetherSuite,
           multidevice::SoftwareFeature::kBetterTogetherHost},
          {mojom::Feature::kInstantTethering,
           multidevice::SoftwareFeature::kInstantTetheringHost},
          {mojom::Feature::kSmartLock,
           multidevice::SoftwareFeature::kSmartLockHost},
          // Note: Most Phone Hub-related features use the same SoftwareFeature.
          {mojom::Feature::kPhoneHub,
           multidevice::SoftwareFeature::kPhoneHubHost},
          {mojom::Feature::kPhoneHubNotifications,
           multidevice::SoftwareFeature::kPhoneHubHost},
          {mojom::Feature::kPhoneHubTaskContinuation,
           multidevice::SoftwareFeature::kPhoneHubHost},
          // Note: Camera Roll is launched separately from the rest of PhoneHub.
          {mojom::Feature::kPhoneHubCameraRoll,
           multidevice::SoftwareFeature::kPhoneHubCameraRollHost},
          {mojom::Feature::kWifiSync,
           multidevice::SoftwareFeature::kWifiSyncHost},
          {mojom::Feature::kEche, multidevice::SoftwareFeature::kEcheHost}};

  for (const auto& pair : kFeatureAndHostSoftwareFeaturePairs) {
    if (pair.first != feature) {
      continue;
    }

    // The bluetooth public address is required in order to use PhoneHub/Eche
    // and its sub-features.
    if ((pair.second == multidevice::SoftwareFeature::kPhoneHubHost ||
         pair.second == multidevice::SoftwareFeature::kEcheHost) &&
        host_device.bluetooth_public_address().empty()) {
      return false;
    }

    multidevice::SoftwareFeatureState feature_state =
        host_device.GetSoftwareFeatureState(pair.second);

    // Edge Case: Eche is considered activated on the host when Phone Hub is
    // enabled and either:
    // * if phone did not specify Eche state via PhoneHub status message:
    //   * Eche's state is kSupported or kEnabled.
    // * if phone did specify Eche state via PhoneHub status message:
    //   * use the specified Eche state.
    if (feature == mojom::Feature::kEche) {
      if (host_device.GetSoftwareFeatureState(
              multidevice::SoftwareFeature::kPhoneHubHost) !=
          multidevice::SoftwareFeatureState::kEnabled) {
        return false;
      }

      EcheSupportReceivedFromPhoneHub eche_support_received_from_phone_hub =
          static_cast<EcheSupportReceivedFromPhoneHub>(
              pref_service_->GetInteger(
                  kEcheOverriddenSupportReceivedFromPhoneHubPrefName));
      switch (eche_support_received_from_phone_hub) {
        case EcheSupportReceivedFromPhoneHub::kNotSpecified:
          return feature_state ==
                     multidevice::SoftwareFeatureState::kSupported ||
                 feature_state == multidevice::SoftwareFeatureState::kEnabled;
        case EcheSupportReceivedFromPhoneHub::kNotSupported:
          return false;
        case EcheSupportReceivedFromPhoneHub::kSupported:
          return true;
      };
    }

    if (feature_state == multidevice::SoftwareFeatureState::kEnabled) {
      return true;
    }

    // Edge Case: features with global states are considered activated on host
    // when the state is kSupported or kEnabled. kEnabled/kSupported correspond
    // to on/off for the global host state.
    return (global_state_feature_managers_.contains(feature) &&
            feature_state == multidevice::SoftwareFeatureState::kSupported);
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

mojom::FeatureState FeatureStateManagerImpl::GetEnabledOrDisabledState(
    mojom::Feature feature) {
  if (global_state_feature_managers_.contains(feature)) {
    return (global_state_feature_managers_.at(feature)->IsFeatureEnabled()
                ? mojom::FeatureState::kEnabledByUser
                : mojom::FeatureState::kDisabledByUser);
  }

  if (!base::Contains(feature_to_enabled_pref_name_map_, feature)) {
    PA_LOG(ERROR) << "FeatureStateManagerImpl::GetEnabledOrDisabledState(): "
                  << "Feature not present in \"enabled pref\" map: " << feature;
    NOTREACHED_IN_MIGRATION();
  }

  return pref_service_->GetBoolean(feature_to_enabled_pref_name_map_[feature])
             ? mojom::FeatureState::kEnabledByUser
             : mojom::FeatureState::kDisabledByUser;
}

void FeatureStateManagerImpl::LogFeatureStates() const {
  base::UmaHistogramEnumeration(
      "MultiDevice.BetterTogetherSuite.MultiDeviceFeatureState",
      cached_feature_state_map_.find(mojom::Feature::kBetterTogetherSuite)
          ->second);
  base::UmaHistogramEnumeration(
      "InstantTethering.MultiDeviceFeatureState",
      cached_feature_state_map_.find(mojom::Feature::kInstantTethering)
          ->second);
  base::UmaHistogramEnumeration(
      "SmartLock.MultiDeviceFeatureState",
      cached_feature_state_map_.find(mojom::Feature::kSmartLock)->second);
  base::UmaHistogramEnumeration(
      "PhoneHub.MultiDeviceFeatureState.TopLevelFeature",
      cached_feature_state_map_.find(mojom::Feature::kPhoneHub)->second);
  base::UmaHistogramEnumeration(
      "PhoneHub.MultiDeviceFeatureState.CameraRoll",
      cached_feature_state_map_.find(mojom::Feature::kPhoneHubCameraRoll)
          ->second);
  base::UmaHistogramEnumeration(
      "PhoneHub.MultiDeviceFeatureState.Notifications",
      cached_feature_state_map_.find(mojom::Feature::kPhoneHubNotifications)
          ->second);
  base::UmaHistogramEnumeration(
      "PhoneHub.MultiDeviceFeatureState.TaskContinuation",
      cached_feature_state_map_.find(mojom::Feature::kPhoneHubTaskContinuation)
          ->second);
  base::UmaHistogramEnumeration(
      "WifiSync.MultiDeviceFeatureState",
      cached_feature_state_map_.find(mojom::Feature::kWifiSync)->second);
  base::UmaHistogramEnumeration(
      "Eche.MultiDeviceFeatureState",
      cached_feature_state_map_.find(mojom::Feature::kEche)->second);
}

}  // namespace multidevice_setup

}  // namespace ash
