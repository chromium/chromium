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
};

inline constexpr std::array<ModelBasedCapabilityKey, 11>
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
  }
}

inline proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    UserVisibleFeatureKey key) {
  return ToModelExecutionFeatureProto(ToModelBasedCapabilityKey(key));
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_
