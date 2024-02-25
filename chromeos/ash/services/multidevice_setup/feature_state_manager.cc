// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/feature_state_manager.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace multidevice_setup {

FeatureStateManager::FeatureStateManager() = default;

FeatureStateManager::~FeatureStateManager() = default;

bool FeatureStateManager::SetFeatureEnabledState(mojom::Feature feature,
                                                 bool enabled) {
  mojom::FeatureState state = GetFeatureStates()[feature];

  // Changing the state is only allowed when changing from enabled to disabled
  // or disabled to enabled.
  if (((state == mojom::FeatureState::kEnabledByUser) && !enabled) ||
      (state == mojom::FeatureState::kDisabledByUser && enabled)) {
    PerformSetFeatureEnabledState(feature, enabled);
    return true;
  }

  PA_LOG(ERROR) << __func__ << ": Cannot set feature " << feature
                << " state from " << state << " to "
                << (enabled ? "enabled" : "disabled");
  return false;
}

void FeatureStateManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FeatureStateManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FeatureStateManager::NotifyFeatureStatesChange(
    const FeatureStatesMap& feature_states_map) {
  for (auto& observer : observer_list_)
    observer.OnFeatureStatesChange(feature_states_map);
}

std::ostream& operator<<(std::ostream& stream,
                         const FeatureStateManager::FeatureStatesMap& map) {
  stream << "{" << std::endl;
  for (const auto& item : map)
    stream << "  " << item.first << ": " << item.second << "," << std::endl;
  stream << "}";
  return stream;
}

}  // namespace multidevice_setup

}  // namespace ash
