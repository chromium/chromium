// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SETTINGS_ENABLED_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SETTINGS_ENABLED_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// Observer to listen to changes in the user opt-in state for a given
// `feature`.
class SettingsEnabledObserver : public base::CheckedObserver {
 public:
  explicit SettingsEnabledObserver(UserVisibleFeatureKey feature);
  ~SettingsEnabledObserver() override;

  // Notifies the consumers whenever the feature enabled state is changed.
  // `is_now_enabled` indicates the current enabled state of the feature. This
  // could be invoked without the enabled change toggling. This is not called
  // automatically when the observer is added initially. Consumers should call
  // `ShouldFeatureBeCurrentlyEnabledForUser` to check the feature state.
  virtual void OnChangeInFeatureCurrentlyEnabledState(bool is_now_enabled) {}

  SettingsEnabledObserver(const SettingsEnabledObserver&) = delete;
  SettingsEnabledObserver& operator=(const SettingsEnabledObserver&) = delete;

  UserVisibleFeatureKey feature() const { return feature_; }

 private:
  const UserVisibleFeatureKey feature_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SETTINGS_ENABLED_OBSERVER_H_
