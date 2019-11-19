// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

base::flat_map<mojom::Feature, std::string>
GenerateFeatureToEnabledPrefNameMap() {
  return base::flat_map<mojom::Feature, std::string>{
      {mojom::Feature::kBetterTogetherSuite,
       kBetterTogetherSuiteEnabledPrefName},
      {mojom::Feature::kInstantTethering, kInstantTetheringEnabledPrefName},
      {mojom::Feature::kMessages, kMessagesEnabledPrefName},
      {mojom::Feature::kSmartLock, kSmartLockEnabledPrefName}};
}

base::flat_map<mojom::Feature, std::string>
GenerateFeatureToAllowedPrefNameMap() {
  return base::flat_map<mojom::Feature, std::string>{
      {mojom::Feature::kInstantTethering, kInstantTetheringAllowedPrefName},
      {mojom::Feature::kMessages, kMessagesAllowedPrefName},
      {mojom::Feature::kSmartLock, kSmartLockAllowedPrefName}};
}

// Each feature's default value is kUnavailableNoVerifiedHost until proven
// otherwise.
base::flat_map<mojom::Feature, mojom::FeatureState>
GenerateInitialDefaultCachedStateMap() {
  return base::flat_map<mojom::Feature, mojom::FeatureState>{
      {mojom::Feature::kBetterTogetherSuite,
       mojom::FeatureState::kUnavailableNoVerifiedHost},
      {mojom::Feature::kInstantTethering,
       mojom::FeatureState::kUnavailableNoVerifiedHost},
      {mojom::Feature::kMessages,
       mojom::FeatureState::kUnavailableNoVerifiedHost},
      {mojom::Feature::kSmartLock,
       mojom::FeatureState::kUnavailableNoVerifiedHost}};
}

void ProcessSuiteEdgeCases(
    FeatureStateManager::FeatureStatesMap* feature_states_map) {
  // First edge case: The Better Together suite does not have its own explicit
  // device policy; instead, if all supported sub-features are prohibited by
  // policy, the entire suite should be considered prohibited.
  bool are_all_sub_features_prohibited = true;
  for (const auto& map_entry : *feature_states_map) {
    // Only check for sub-features.
    if (map_entry.first == mojom::Feature::kBetterTogetherSuite)
      continue;

    if (map_entry.second != mojom::FeatureState::kProhibitedByPolicy) {
      are_all_sub_features_prohibited = false;
      break;
    }
  }

  if (are_all_sub_features_prohibited) {
    (*feature_states_map)[mojom::Feature::kBetterTogetherSuite] =
        mojom::FeatureState::kProhibitedByPolicy;
    return;
  }

  // Second edge case: If the Better Together suite is disabled by the user, any
  // sub-features which have been enabled by the user should be unavailable. In
  // this context, the suite serves as a gatekeeper to all sub-features.
  if ((*feature_states_map)[mojom::Feature::kBetterTogetherSuite] !=
      mojom::FeatureState::kDisabledByUser)
    return;

  for (auto& map_entry : *feature_states_map) {
    if (map_entry.second == mojom::FeatureState::kEnabledByUser ||
        map_entry.second == mojom::FeatureState::kFurtherSetupRequired) {
      map_entry.second = mojom::FeatureState::kUnavailableSuiteDisabled;
    }
  }
}

bool HasFeatureStateChanged(
    const base::Optional<FeatureStateManager::FeatureStatesMap>&
        previous_states,
    const FeatureStateManager::FeatureStatesMap& new_states,
    mojom::Feature feature) {
  if (!previous_states)
    return true;
  return previous_states->find(feature)->second !=
         new_states.find(feature)->second;
}

void LogFeatureStates(
    const base::Optional<FeatureStateManager::FeatureStatesMap>&
        previous_states,
    const FeatureStateManager::FeatureStatesMap& new_states) {
  if (HasFeatureStateChanged(previous_states, new_states,
                             mojom::Feature::kBetterTogetherSuite)) {
    UMA_HISTOGRAM_ENUMERATION(
        "MultiDevice.BetterTogetherSuite.MultiDeviceFeatureState",
        new_states.find(mojom::Feature::kBetterTogetherSuite)->second);
  }

  if (HasFeatureStateChanged(previous_states, new_states,
                             mojom::Feature::kInstantTethering)) {
    UMA_HISTOGRAM_ENUMERATION(
        "InstantTethering.MultiDeviceFeatureState",
        new_states.find(mojom::Feature::kInstantTethering)->second);
  }

  if (HasFeatureStateChanged(previous_states, new_states,
                             mojom::Feature::kMessages)) {
    UMA_HISTOGRAM_ENUMERATION(
        "AndroidSms.MultiDeviceFeatureState",
        new_states.find(mojom::Feature::kMessages)->second);
  }

  if (HasFeatureStateChanged(previous_states, new_states,
                             mojom::Feature::kSmartLock)) {
    UMA_HISTOGRAM_ENUMERATION(
        "SmartLock.MultiDeviceFeatureState",
        new_states.find(mojom::Feature::kSmartLock)->second);
  }
}

}  // namespace

// static
FeatureStateManagerImpl::Factory*
    FeatureStateManagerImpl::Factory::test_factory_ = nullptr;

// static
FeatureStateManagerImpl::Factory* FeatureStateManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void FeatureStateManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

FeatureStateManagerImpl::Factory::~Factory() = default;

std::unique_ptr<FeatureStateManager>
FeatureStateManagerImpl::Factory::BuildInstance(
    PrefService* pref_service,
    HostStatusProvider* host_status_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker) {
  return base::WrapUnique(new FeatureStateManagerImpl(
      pref_service, host_status_provider, device_sync_client,
      android_sms_pairing_state_tracker));
}

FeatureStateManagerImpl::FeatureStateManagerImpl(
    PrefService* pref_service,
    HostStatusProvider* host_status_provider,
    device_sync::DeviceSyncClient* device_sync_client,
    AndroidSmsPairingStateTracker* android_sms_pairing_state_tracker)
    : pref_service_(pref_service),
      host_status_provider_(host_status_provider),
      device_sync_client_(device_sync_client),
      android_sms_pairing_state_tracker_(android_sms_pairing_state_tracker),
      feature_to_enabled_pref_name_map_(GenerateFeatureToEnabledPrefNameMap()),
      feature_to_allowed_pref_name_map_(GenerateFeatureToAllowedPrefNameMap()),
      cached_feature_state_map_(GenerateInitialDefaultCachedStateMap()) {
  host_status_provider_->AddObserver(this);
  device_sync_client_->AddObserver(this);
  if (android_sms_pairing_state_tracker_)
    android_sms_pairing_state_tracker_->AddObserver(this);

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

  // Prime the cache. Since this is the initial computation, it does not
  // represent a true change of feature state values, so observers should not be
  // notified.
  UpdateFeatureStateCache(false /* notify_observers_of_changes */);

  LogFeatureStates(base::nullopt /* previous_states */,
                   cached_feature_state_map_ /* new_states */);
}

FeatureStateManagerImpl::~FeatureStateManagerImpl() {
  host_status_provider_->RemoveObserver(this);
  device_sync_client_->RemoveObserver(this);
  if (android_sms_pairing_state_tracker_)
    android_sms_pairing_state_tracker_->RemoveObserver(this);
}

FeatureStateManager::FeatureStatesMap
FeatureStateManagerImpl::GetFeatureStates() {
  return cached_feature_state_map_;
}

void FeatureStateManagerImpl::PerformSetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled) {
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

  if (previous_cached_feature_state_map == cached_feature_state_map_)
    return;
  PA_LOG(VERBOSE) << "Feature states map changed. Old map: "
                  << previous_cached_feature_state_map
                  << ", new map: " << cached_feature_state_map_;
  LogFeatureStates(previous_cached_feature_state_map /* previous_states */,
                   cached_feature_state_map_ /* new_states */);
  NotifyFeatureStatesChange(cached_feature_state_map_);
}

mojom::FeatureState FeatureStateManagerImpl::ComputeFeatureState(
    mojom::Feature feature) {
  if (!IsAllowedByPolicy(feature))
    return mojom::FeatureState::kProhibitedByPolicy;

  HostStatusProvider::HostStatusWithDevice status_with_device =
      host_status_provider_->GetHostWithStatus();

  if (status_with_device.host_status() != mojom::HostStatus::kHostVerified)
    return mojom::FeatureState::kUnavailableNoVerifiedHost;

  if (!IsSupportedByChromebook(feature))
    return mojom::FeatureState::kNotSupportedByChromebook;

  if (!HasSufficientSecurity(feature, *status_with_device.host_device()))
    return mojom::FeatureState::kUnavailableInsufficientSecurity;

  if (!HasBeenActivatedByPhone(feature, *status_with_device.host_device()))
    return mojom::FeatureState::kNotSupportedByPhone;

  if (RequiresFurtherSetup(feature))
    return mojom::FeatureState::kFurtherSetupRequired;

  return GetEnabledOrDisabledState(feature);
}

bool FeatureStateManagerImpl::IsAllowedByPolicy(mojom::Feature feature) {
  // If no policy preference exists for this feature, the feature is implicitly
  // allowed.
  if (!base::Contains(feature_to_allowed_pref_name_map_, feature))
    return true;

  return pref_service_->GetBoolean(feature_to_allowed_pref_name_map_[feature]);
}

bool FeatureStateManagerImpl::IsSupportedByChromebook(mojom::Feature feature) {
  static const std::pair<mojom::Feature, multidevice::SoftwareFeature>
      kFeatureAndClientSoftwareFeaturePairs[] = {
          {mojom::Feature::kBetterTogetherSuite,
           multidevice::SoftwareFeature::kBetterTogetherClient},
          {mojom::Feature::kInstantTethering,
           multidevice::SoftwareFeature::kInstantTetheringClient},
          {mojom::Feature::kMessages,
           multidevice::SoftwareFeature::kMessagesForWebClient},
          {mojom::Feature::kSmartLock,
           multidevice::SoftwareFeature::kSmartLockClient}};

  for (const auto& pair : kFeatureAndClientSoftwareFeaturePairs) {
    if (pair.first != feature)
      continue;

    return device_sync_client_->GetLocalDeviceMetadata()
               ->GetSoftwareFeatureState(pair.second) !=
           multidevice::SoftwareFeatureState::kNotSupported;
  }

  NOTREACHED();
  return false;
}

bool FeatureStateManagerImpl::HasSufficientSecurity(
    mojom::Feature feature,
    const multidevice::RemoteDeviceRef& host_device) {
  if (feature != mojom::Feature::kSmartLock)
    return true;

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
          {mojom::Feature::kMessages,
           multidevice::SoftwareFeature::kMessagesForWebHost},
          {mojom::Feature::kSmartLock,
           multidevice::SoftwareFeature::kSmartLockHost}};

  for (const auto& pair : kFeatureAndHostSoftwareFeaturePairs) {
    if (pair.first != feature)
      continue;

    return host_device.GetSoftwareFeatureState(pair.second) ==
           multidevice::SoftwareFeatureState::kEnabled;
  }

  NOTREACHED();
  return false;
}

bool FeatureStateManagerImpl::RequiresFurtherSetup(mojom::Feature feature) {
  if (feature != mojom::Feature::kMessages)
    return false;

  if (GetEnabledOrDisabledState(feature) ==
      mojom::FeatureState::kDisabledByUser) {
    return false;
  }

  return android_sms_pairing_state_tracker_ &&
         !android_sms_pairing_state_tracker_->IsAndroidSmsPairingComplete();
}

mojom::FeatureState FeatureStateManagerImpl::GetEnabledOrDisabledState(
    mojom::Feature feature) {
  if (!base::Contains(feature_to_enabled_pref_name_map_, feature)) {
    PA_LOG(ERROR) << "FeatureStateManagerImpl::GetEnabledOrDisabledState(): "
                  << "Feature not present in \"enabled pref\" map: " << feature;
    NOTREACHED();
  }

  return pref_service_->GetBoolean(feature_to_enabled_pref_name_map_[feature])
             ? mojom::FeatureState::kEnabledByUser
             : mojom::FeatureState::kDisabledByUser;
}

}  // namespace multidevice_setup

}  // namespace chromeos
