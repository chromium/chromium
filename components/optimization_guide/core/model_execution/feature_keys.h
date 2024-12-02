// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_

#include <array>
#include <optional>
#include <ostream>

#include "base/notreached.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

// Capabilities that are implemented by model execution.
enum class ModelBasedCapabilityKey {
  kCompose = proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE,
  kTabOrganization =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION,
  kWallpaperSearch =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH,
  kTest = proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST,
  kTextSafety =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEXT_SAFETY,
  kPromptApi = proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PROMPT_API,
  kHistorySearch =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_HISTORY_SEARCH,
  kSummarize = proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SUMMARIZE,
  kFormsPredictions =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_FORMS_PREDICTIONS,
  kFormsAnnotations =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_FORMS_ANNOTATIONS,
  kHistoryQueryIntent = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_HISTORY_QUERY_INTENT,
  kBlingPrototyping =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_BLING_PROTOTYPING,
  kPasswordChangeSubmission = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_PASSWORD_CHANGE_SUBMISSION,
  kScamDetection =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
};

inline std::ostream& operator<<(std::ostream& out,
                                const ModelBasedCapabilityKey& val) {
  switch (val) {
    case ModelBasedCapabilityKey::kCompose:
      return out << "Compose";
    case ModelBasedCapabilityKey::kTabOrganization:
      return out << "TabOrganization";
    case ModelBasedCapabilityKey::kWallpaperSearch:
      return out << "WallpaperSearch";
    case ModelBasedCapabilityKey::kTest:
      return out << "Test";
    case ModelBasedCapabilityKey::kTextSafety:
      return out << "TextSafety";
    case ModelBasedCapabilityKey::kPromptApi:
      return out << "PromptApi";
    case ModelBasedCapabilityKey::kHistorySearch:
      return out << "HistorySearch";
    case ModelBasedCapabilityKey::kSummarize:
      return out << "Summarize";
    case ModelBasedCapabilityKey::kFormsPredictions:
      return out << "FormsPredictions";
    case ModelBasedCapabilityKey::kFormsAnnotations:
      return out << "FormsAnnotations";
    case ModelBasedCapabilityKey::kHistoryQueryIntent:
      return out << "HistoryQueryIntent";
    case ModelBasedCapabilityKey::kBlingPrototyping:
      return out << "BlingPrototyping";
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return out << "PasswordChangeSubmission";
    case ModelBasedCapabilityKey::kScamDetection:
      return out << "ScamDetection";
  }
  return out;
}

inline constexpr std::array<ModelBasedCapabilityKey, 14>
    kAllModelBasedCapabilityKeys = {
        ModelBasedCapabilityKey::kCompose,
        ModelBasedCapabilityKey::kTabOrganization,
        ModelBasedCapabilityKey::kWallpaperSearch,
        ModelBasedCapabilityKey::kTest,
        ModelBasedCapabilityKey::kTextSafety,
        ModelBasedCapabilityKey::kPromptApi,
        ModelBasedCapabilityKey::kHistorySearch,
        ModelBasedCapabilityKey::kSummarize,
        ModelBasedCapabilityKey::kFormsPredictions,
        ModelBasedCapabilityKey::kFormsAnnotations,
        ModelBasedCapabilityKey::kHistoryQueryIntent,
        ModelBasedCapabilityKey::kBlingPrototyping,
        ModelBasedCapabilityKey::kPasswordChangeSubmission,
        ModelBasedCapabilityKey::kScamDetection,
};

// A "real" feature implemented by a model-based capability.
// These will have their own prefs / settings / policies etc.
enum class UserVisibleFeatureKey {
  kCompose = static_cast<int>(ModelBasedCapabilityKey::kCompose),
  kTabOrganization =
      static_cast<int>(ModelBasedCapabilityKey::kTabOrganization),
  kWallpaperSearch =
      static_cast<int>(ModelBasedCapabilityKey::kWallpaperSearch),
  kHistorySearch = static_cast<int>(ModelBasedCapabilityKey::kHistorySearch),
};

inline constexpr std::array<UserVisibleFeatureKey, 4>
    kAllUserVisibleFeatureKeys = {
        UserVisibleFeatureKey::kCompose,
        UserVisibleFeatureKey::kTabOrganization,
        UserVisibleFeatureKey::kWallpaperSearch,
        UserVisibleFeatureKey::kHistorySearch,
};

inline ModelBasedCapabilityKey ToModelBasedCapabilityKey(
    UserVisibleFeatureKey key) {
  switch (key) {
    case UserVisibleFeatureKey::kCompose:
      return ModelBasedCapabilityKey::kCompose;
    case UserVisibleFeatureKey::kTabOrganization:
      return ModelBasedCapabilityKey::kTabOrganization;
    case UserVisibleFeatureKey::kWallpaperSearch:
      return ModelBasedCapabilityKey::kWallpaperSearch;
    case UserVisibleFeatureKey::kHistorySearch:
      return ModelBasedCapabilityKey::kHistorySearch;
  }
}

inline ModelBasedCapabilityKey ToModelBasedCapabilityKey(
    proto::ModelExecutionFeature feature) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      return ModelBasedCapabilityKey::kCompose;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      return ModelBasedCapabilityKey::kTabOrganization;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      return ModelBasedCapabilityKey::kWallpaperSearch;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST:
      return ModelBasedCapabilityKey::kTest;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEXT_SAFETY:
      return ModelBasedCapabilityKey::kTextSafety;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PROMPT_API:
      return ModelBasedCapabilityKey::kPromptApi;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_HISTORY_SEARCH:
      return ModelBasedCapabilityKey::kHistorySearch;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_FORMS_PREDICTIONS:
      return ModelBasedCapabilityKey::kFormsPredictions;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_FORMS_ANNOTATIONS:
      return ModelBasedCapabilityKey::kFormsAnnotations;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SUMMARIZE:
      return ModelBasedCapabilityKey::kSummarize;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_HISTORY_QUERY_INTENT:
      return ModelBasedCapabilityKey::kHistoryQueryIntent;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_BLING_PROTOTYPING:
      return ModelBasedCapabilityKey::kBlingPrototyping;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_PASSWORD_CHANGE_SUBMISSION:
        return ModelBasedCapabilityKey::kPasswordChangeSubmission;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION:
      return ModelBasedCapabilityKey::kScamDetection;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      NOTREACHED() << "Invalid feature";
  }
}

inline proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    ModelBasedCapabilityKey key) {
  switch (key) {
    case ModelBasedCapabilityKey::kCompose:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE;
    case ModelBasedCapabilityKey::kTabOrganization:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION;
    case ModelBasedCapabilityKey::kWallpaperSearch:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH;
    case ModelBasedCapabilityKey::kTest:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST;
    case ModelBasedCapabilityKey::kTextSafety:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEXT_SAFETY;
    case ModelBasedCapabilityKey::kPromptApi:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PROMPT_API;
    case ModelBasedCapabilityKey::kSummarize:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SUMMARIZE;
    case ModelBasedCapabilityKey::kHistorySearch:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_HISTORY_SEARCH;
    case ModelBasedCapabilityKey::kFormsPredictions:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_FORMS_PREDICTIONS;
    case ModelBasedCapabilityKey::kFormsAnnotations:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_FORMS_ANNOTATIONS;
    case ModelBasedCapabilityKey::kHistoryQueryIntent:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_HISTORY_QUERY_INTENT;
    case ModelBasedCapabilityKey::kBlingPrototyping:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_BLING_PROTOTYPING;
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_PASSWORD_CHANGE_SUBMISSION;
    case ModelBasedCapabilityKey::kScamDetection:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_SCAM_DETECTION;
  }
}

inline proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    UserVisibleFeatureKey key) {
  return ToModelExecutionFeatureProto(ToModelBasedCapabilityKey(key));
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_
