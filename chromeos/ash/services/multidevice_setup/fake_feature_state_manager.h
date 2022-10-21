// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_MANAGER_H_

#include "chromeos/ash/services/multidevice_setup/feature_state_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {

namespace multidevice_setup {

// Test FeatureStateManager implementation. This class initializes all features
// to be state mojom::FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts.
class FakeFeatureStateManager : public FeatureStateManager {
 public:
  FakeFeatureStateManager();

  FakeFeatureStateManager(const FakeFeatureStateManager&) = delete;
  FakeFeatureStateManager& operator=(const FakeFeatureStateManager&) = delete;

  ~FakeFeatureStateManager() override;

  mojom::FeatureState GetFeatureState(mojom::Feature feature);
  void SetFeatureState(mojom::Feature feature, mojom::FeatureState state);
  void SetFeatureStates(const FeatureStatesMap& feature_states_map);

  using FeatureStateManager::NotifyFeatureStatesChange;

 private:
  // FeatureStateManager:
  FeatureStatesMap GetFeatureStates() override;
  void PerformSetFeatureEnabledState(mojom::Feature feature,
                                     bool enabled) override;

  FeatureStatesMap feature_states_map_;
};

// Test FeatureStateManager::Observer implementation.
class FakeFeatureStateManagerObserver : public FeatureStateManager::Observer {
 public:
  FakeFeatureStateManagerObserver();

  FakeFeatureStateManagerObserver(const FakeFeatureStateManagerObserver&) =
      delete;
  FakeFeatureStateManagerObserver& operator=(
      const FakeFeatureStateManagerObserver&) = delete;

  ~FakeFeatureStateManagerObserver() override;

  const std::vector<FeatureStateManager::FeatureStatesMap>&
  feature_state_updates() const {
    return feature_state_updates_;
  }

 private:
  // FeatureStateManager::Observer:
  void OnFeatureStatesChange(
      const FeatureStateManager::FeatureStatesMap& feature_states_map) override;

  std::vector<FeatureStateManager::FeatureStatesMap> feature_state_updates_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_MANAGER_H_
