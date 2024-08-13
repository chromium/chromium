// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

MqlsFeatureMetadata::MqlsFeatureMetadata(
    std::string name,
    proto::LogAiDataRequest::FeatureCase logging_feature_case,
    EnterprisePolicyPref enterprise_policy,
    const base::Feature* field_trial_feature,
    UserFeedbackCallback get_user_feedback_callback,
    std::optional<UserVisibleFeatureKey> user_visible_feature_key)
    : name_(name),
      logging_feature_case_(logging_feature_case),
      enterprise_policy_(enterprise_policy),
      field_trial_feature_(field_trial_feature),
      get_user_feedback_callback_(get_user_feedback_callback),
      user_visible_feature_key_(user_visible_feature_key) {
  CHECK(base::IsStringASCII(name));
  CHECK(field_trial_feature_);
  CHECK(get_user_feedback_callback);
}

MqlsFeatureMetadata::~MqlsFeatureMetadata() = default;

bool MqlsFeatureMetadata::LoggingEnabledViaFieldTrial() const {
  return base::FeatureList::IsEnabled(*field_trial_feature_);
}

MqlsFeatureRegistry::MqlsFeatureRegistry() = default;

MqlsFeatureRegistry::~MqlsFeatureRegistry() = default;

MqlsFeatureRegistry& MqlsFeatureRegistry::GetInstance() {
  static base::NoDestructor<MqlsFeatureRegistry> registry;
  return *registry;
}

void MqlsFeatureRegistry::Register(
    std::unique_ptr<MqlsFeatureMetadata> new_metadata) {
  for (auto& metadata : features_) {
    CHECK(metadata->logging_feature_case() !=
          new_metadata->logging_feature_case());
    CHECK(metadata->name() != new_metadata->name());
  }
  features_.emplace_back(std::move(new_metadata));
}

const MqlsFeatureMetadata* MqlsFeatureRegistry::GetFeature(
    proto::LogAiDataRequest::FeatureCase feature_case) const {
  for (auto& metadata : features_) {
    if (metadata->logging_feature_case() == feature_case) {
      return metadata.get();
    }
  }
  return nullptr;
}

void MqlsFeatureRegistry::ClearForTesting() {
  features_.clear();
}

}  // namespace optimization_guide
