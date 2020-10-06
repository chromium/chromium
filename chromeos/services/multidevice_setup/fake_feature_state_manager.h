// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_MANAGER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromeos/services/multidevice_setup/feature_state_manager.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// Test FeatureStateManager implementation. This class initializes all features
// to be state mojom::FeatureState::kUnavailableNoVerifiedHost.
class FakeFeatureStateManager : public FeatureStateManager {
 public:
  FakeFeatureStateManager();
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

  DISALLOW_COPY_AND_ASSIGN(FakeFeatureStateManager);
};

// Test FeatureStateManager::Observer implementation.
class FakeFeatureStateManagerObserver : public FeatureStateManager::Observer {
 public:
  FakeFeatureStateManagerObserver();
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

  DISALLOW_COPY_AND_ASSIGN(FakeFeatureStateManagerObserver);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_MANAGER_H_
