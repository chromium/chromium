// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_MQLS_FEATURE_REGISTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_MQLS_FEATURE_REGISTRY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace optimization_guide {

namespace proto {
class LogAiDataRequest;
}  // namespace proto

using UserFeedbackCallback =
    base::RepeatingCallback<proto::UserFeedback(proto::LogAiDataRequest&)>;

// MqlsFeatureMetadata holds metadata for each proto logged using the Model
// Quality Logging Service (MQLS). Note that for a given user-visible feature,
// there may be multiple types of protos logged.
class MqlsFeatureMetadata {
 public:
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  MqlsFeatureMetadata(
      std::string name,
      proto::LogAiDataRequest::FeatureCase logging_feature_case,
      // TODO(b/354705998): add network annotation.
      EnterprisePolicyPref enterprise_policy,
      const base::Feature* field_trial_feature,
      UserFeedbackCallback get_user_feedback_callback,
      std::optional<UserVisibleFeatureKey> user_visible_feature_key);

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  ~MqlsFeatureMetadata();

  // Returns whether logging is allowed according to the Field Trial config.
  // Note that even if this returns true, logging may still be disallowed due to
  // the enterprise policy.
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  bool LoggingEnabledViaFieldTrial() const;

  std::string name() const { return name_; }

  EnterprisePolicyPref enterprise_policy() const { return enterprise_policy_; }

  proto::LogAiDataRequest::FeatureCase logging_feature_case() const {
    return logging_feature_case_;
  }

  std::optional<UserVisibleFeatureKey> user_visible_feature_key() const {
    return user_visible_feature_key_;
  }

  const base::Feature* field_trial_feature() const {
    return field_trial_feature_;
  }

  UserFeedbackCallback get_user_feedback_callback() const {
    return get_user_feedback_callback_;
  }

 private:
  // Name of the feature for histograms. This should only contain ASCII
  // characters.
  std::string name_;

  // The enum representing the logging proto this metadata controls.
  proto::LogAiDataRequest::FeatureCase logging_feature_case_;

  // The pref to control the enterprise policy setting of the feature.
  EnterprisePolicyPref enterprise_policy_;

  // The base::Feature that controls whether logging is currently enabled for
  // this feature. This must always be non-null: all features using MQLS
  // should support disabling logging via Field Trial configs.
  const raw_ptr<const base::Feature> field_trial_feature_;

  // A function that extracts UserFeedback enum (thumbs up/down information)
  // from the logging proto. Each feature may store their user feedback status
  // in a different way, so this function allows the logging code to collect
  // data for a generic user feedback histogram.
  UserFeedbackCallback get_user_feedback_callback_;

  // If the feature relies on the chrome://settings/ai page, this contains the
  // UserVisibleFeatureKey enum value for that feature. This makes sure logging
  // is disabled if the user disables the feature via the chrome://settings/ai
  // page.
  std::optional<UserVisibleFeatureKey> user_visible_feature_key_;
};

class MqlsFeatureRegistry {
 public:
  MqlsFeatureRegistry();
  ~MqlsFeatureRegistry();

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  static MqlsFeatureRegistry& GetInstance();

  // Registers a feature to use MQLS. Features that want to use this should
  // register themselves in
  // components/optimization_guide/core/feature_registry/feature_registration.cc.
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  void Register(std::unique_ptr<MqlsFeatureMetadata> feature_metadata);

  // Get the metadata for the given feature. Returns nullptr if there is no
  // registered feature matching the enum value.
  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  const MqlsFeatureMetadata* GetFeature(
      proto::LogAiDataRequest::FeatureCase) const;

  COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
  void ClearForTesting();

 private:
  std::vector<std::unique_ptr<MqlsFeatureMetadata>> features_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_FEATURE_REGISTRY_MQLS_FEATURE_REGISTRY_H_
