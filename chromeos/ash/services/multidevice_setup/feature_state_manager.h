// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FEATURE_STATE_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FEATURE_STATE_MANAGER_H_

#include <ostream>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {

namespace multidevice_setup {

// Tracks the state of the multi-device features, providing a getter/observer
// interface as well as a way to toggle a feature on/off when appropriate.
class FeatureStateManager {
 public:
  using FeatureStatesMap = base::flat_map<mojom::Feature, mojom::FeatureState>;

  class Observer {
   public:
    virtual ~Observer() = default;

    // Invoked when one or more features have changed state.
    virtual void OnFeatureStatesChange(
        const FeatureStatesMap& feature_states_map) = 0;
  };

  FeatureStateManager(const FeatureStateManager&) = delete;
  FeatureStateManager& operator=(const FeatureStateManager&) = delete;

  virtual ~FeatureStateManager();

  virtual FeatureStatesMap GetFeatureStates() = 0;

  // Attempts to enable or disable the feature; returns whether this operation
  // succeeded. A feature can only be changed via this function if the current
  // state is mojom::FeatureState::kEnabledByUser or
  // mojom::FeatureState::kDisabledByUser.
  bool SetFeatureEnabledState(mojom::Feature feature, bool enabled);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  FeatureStateManager();

  // Enables or disables the feature; by the time this function has been called,
  // it has already been confirmed that the state is indeed able to be changed.
  virtual void PerformSetFeatureEnabledState(mojom::Feature feature,
                                             bool enabled) = 0;

  void NotifyFeatureStatesChange(const FeatureStatesMap& feature_states_map);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

std::ostream& operator<<(std::ostream& stream,
                         const FeatureStateManager::FeatureStatesMap& map);

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_FEATURE_STATE_MANAGER_H_
