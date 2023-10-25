// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/fake_feature_state_manager.h"

namespace ash {

namespace multidevice_setup {

namespace {

// Each feature's default value is kUnavailableNoVerifiedHost_NoEligibleHosts
// until proven otherwise.
FeatureStateManager::FeatureStatesMap GenerateInitialDefaultCachedStateMap() {
  return FeatureStateManager::FeatureStatesMap{
      {mojom::Feature::kBetterTogetherSuite,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kInstantTethering,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kMessages,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts},
      {mojom::Feature::kSmartLock,
       mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts}};
}

}  // namespace

FakeFeatureStateManager::FakeFeatureStateManager()
    : feature_states_map_(GenerateInitialDefaultCachedStateMap()) {}

FakeFeatureStateManager::~FakeFeatureStateManager() = default;

mojom::FeatureState FakeFeatureStateManager::GetFeatureState(
    mojom::Feature feature) {
  return feature_states_map_[feature];
}

void FakeFeatureStateManager::SetFeatureState(mojom::Feature feature,
                                              mojom::FeatureState state) {
  if (feature_states_map_[feature] == state)
    return;

  feature_states_map_[feature] = state;
  NotifyFeatureStatesChange(feature_states_map_);
}

void FakeFeatureStateManager::SetFeatureStates(
    const FeatureStatesMap& feature_states_map) {
  if (feature_states_map_ == feature_states_map)
    return;

  feature_states_map_ = feature_states_map;
  NotifyFeatureStatesChange(feature_states_map_);
}

FeatureStateManager::FeatureStatesMap
FakeFeatureStateManager::GetFeatureStates() {
  return feature_states_map_;
}

void FakeFeatureStateManager::PerformSetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled) {
  if (enabled)
    SetFeatureState(feature, mojom::FeatureState::kEnabledByUser);
  else
    SetFeatureState(feature, mojom::FeatureState::kDisabledByUser);
}

FakeFeatureStateManagerObserver::FakeFeatureStateManagerObserver() = default;

FakeFeatureStateManagerObserver::~FakeFeatureStateManagerObserver() = default;

void FakeFeatureStateManagerObserver::OnFeatureStatesChange(
    const FeatureStateManager::FeatureStatesMap& feature_states_map) {
  feature_state_updates_.emplace_back(feature_states_map);
}

}  // namespace multidevice_setup

}  // namespace ash
