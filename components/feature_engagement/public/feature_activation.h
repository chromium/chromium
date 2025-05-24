// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_ACTIVATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_ACTIVATION_H_

#include <optional>
#include <string>

namespace feature_engagement {

class FeatureActivation {
 public:
  enum class State {
    // All features are enabled
    kAllEnabled,
    // All features are disabled
    kAllDisabled,
    // A single feature is enabled
    kSingleFeatureEnabled,
  };

  FeatureActivation(FeatureActivation&& feature_activation);
  FeatureActivation(FeatureActivation& feature_activation);
  // The constructor to use when a single feature is activated.
  explicit FeatureActivation(std::string unique_feature_name);
  ~FeatureActivation();

  // Factory for the all enabled state.
  static FeatureActivation AllEnabled();
  // Factory for the all disabled state.
  static FeatureActivation AllDisabled();

  // Returns the name of the unique feature. CHECK fail if there is not a unique
  // feature.
  const std::string& get_unique_feature_name();

  // Returns the state.
  State get_state();

 private:
  explicit FeatureActivation(bool all_enbled);

  const State state_;
  // The name of the unique feature, if the state is `kSingleFeatureEnabled`.
  const std::optional<std::string> unique_feature_name_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_ACTIVATION_H_
