// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_FEATURE_OVERRIDES_H_
#define COMPONENTS_VARIATIONS_FEATURE_OVERRIDES_H_

#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"

namespace variations {

// Utility class for overriding feature states on a base::FeatureList.
class COMPONENT_EXPORT(VARIATIONS) FeatureOverrides {
 public:
  explicit FeatureOverrides(base::FeatureList& feature_list);

  FeatureOverrides(const FeatureOverrides& other) = delete;
  FeatureOverrides& operator=(const FeatureOverrides& other) = delete;

  ~FeatureOverrides();

  // Enables a feature with WebView-specific override.
  void EnableFeature(const base::Feature& feature);

  // Disables a feature with WebView-specific override.
  void DisableFeature(const base::Feature& feature);

  // Enables or disable a feature with a field trial. This can be used for
  // setting feature parameters.
  void OverrideFeatureWithFieldTrial(
      const base::Feature& feature,
      base::FeatureList::OverrideState override_state,
      base::FieldTrial* field_trial);

 private:
  struct FieldTrialOverride {
    raw_ref<const base::Feature> feature;
    base::FeatureList::OverrideState override_state;
    raw_ptr<base::FieldTrial> field_trial;
  };

  raw_ref<base::FeatureList> feature_list_;
  std::vector<base::FeatureList::FeatureOverrideInfo> overrides_;
  std::vector<FieldTrialOverride> field_trial_overrides_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_FEATURE_OVERRIDES_H_
