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
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

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
  kFormsClassifications = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_FORMS_CLASSIFICATIONS,
  kHistoryQueryIntent = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_HISTORY_QUERY_INTENT,
  kBlingPrototyping =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_BLING_PROTOTYPING,
  kPasswordChangeSubmission = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_PASSWORD_CHANGE_SUBMISSION,
  kScamDetection =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
  kPermissionsAi =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PERMISSIONS_AI,
  kProofreaderApi =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PROOFREADER_API,
  kWritingAssistanceApi = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API,
  kEnhancedCalendar =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_ENHANCED_CALENDAR,
  kZeroStateSuggestions = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_ZERO_STATE_SUGGESTIONS,
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
    case ModelBasedCapabilityKey::kFormsClassifications:
      return out << "FormsClassifications";
    case ModelBasedCapabilityKey::kHistoryQueryIntent:
      return out << "HistoryQueryIntent";
    case ModelBasedCapabilityKey::kBlingPrototyping:
      return out << "BlingPrototyping";
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return out << "PasswordChangeSubmission";
    case ModelBasedCapabilityKey::kScamDetection:
      return out << "ScamDetection";
    case ModelBasedCapabilityKey::kPermissionsAi:
      return out << "PermissionsAi";
    case ModelBasedCapabilityKey::kProofreaderApi:
      return out << "ProofreaderApi";
    case ModelBasedCapabilityKey::kWritingAssistanceApi:
      return out << "WritingAssistanceApi";
    case ModelBasedCapabilityKey::kEnhancedCalendar:
      return out << "EnhancedCalendar";
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return out << "ZeroStateSuggestions";
  }
  return out;
}

inline constexpr auto kAllModelBasedCapabilityKeys =
    std::to_array<ModelBasedCapabilityKey>({
        ModelBasedCapabilityKey::kCompose,
        ModelBasedCapabilityKey::kTabOrganization,
        ModelBasedCapabilityKey::kWallpaperSearch,
        ModelBasedCapabilityKey::kTest,
        ModelBasedCapabilityKey::kTextSafety,
        ModelBasedCapabilityKey::kPromptApi,
        ModelBasedCapabilityKey::kHistorySearch,
        ModelBasedCapabilityKey::kSummarize,
        ModelBasedCapabilityKey::kFormsClassifications,
        ModelBasedCapabilityKey::kHistoryQueryIntent,
        ModelBasedCapabilityKey::kBlingPrototyping,
        ModelBasedCapabilityKey::kPasswordChangeSubmission,
        ModelBasedCapabilityKey::kScamDetection,
        ModelBasedCapabilityKey::kPermissionsAi,
        ModelBasedCapabilityKey::kProofreaderApi,
        ModelBasedCapabilityKey::kWritingAssistanceApi,
        ModelBasedCapabilityKey::kEnhancedCalendar,
        ModelBasedCapabilityKey::kZeroStateSuggestions,
    });

// A "real" feature implemented by a model-based capability.
// These will have their own prefs / settings / policies etc.
enum class UserVisibleFeatureKey {
  kCompose = static_cast<int>(ModelBasedCapabilityKey::kCompose),
  kTabOrganization =
      static_cast<int>(ModelBasedCapabilityKey::kTabOrganization),
  kWallpaperSearch =
      static_cast<int>(ModelBasedCapabilityKey::kWallpaperSearch),
  kHistorySearch = static_cast<int>(ModelBasedCapabilityKey::kHistorySearch),
  kPasswordChangeSubmission =
      static_cast<int>(ModelBasedCapabilityKey::kPasswordChangeSubmission),
};

inline constexpr auto kAllUserVisibleFeatureKeys =
    std::to_array<UserVisibleFeatureKey>({
        UserVisibleFeatureKey::kCompose,
        UserVisibleFeatureKey::kTabOrganization,
        UserVisibleFeatureKey::kWallpaperSearch,
        UserVisibleFeatureKey::kHistorySearch,
        UserVisibleFeatureKey::kPasswordChangeSubmission,
    });

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
    case UserVisibleFeatureKey::kPasswordChangeSubmission:
      return ModelBasedCapabilityKey::kPasswordChangeSubmission;
  }
}

inline ModelBasedCapabilityKey ToModelBasedCapabilityKey(
    mojom::ModelBasedCapabilityKey key) {
  switch (key) {
    case mojom::ModelBasedCapabilityKey::kCompose:
      return ModelBasedCapabilityKey::kCompose;
    case mojom::ModelBasedCapabilityKey::kTabOrganization:
      return ModelBasedCapabilityKey::kTabOrganization;
    case mojom::ModelBasedCapabilityKey::kWallpaperSearch:
      return ModelBasedCapabilityKey::kWallpaperSearch;
    case mojom::ModelBasedCapabilityKey::kTest:
      return ModelBasedCapabilityKey::kTest;
    case mojom::ModelBasedCapabilityKey::kTextSafety:
      return ModelBasedCapabilityKey::kTextSafety;
    case mojom::ModelBasedCapabilityKey::kPromptApi:
      return ModelBasedCapabilityKey::kPromptApi;
    case mojom::ModelBasedCapabilityKey::kHistorySearch:
      return ModelBasedCapabilityKey::kHistorySearch;
    case mojom::ModelBasedCapabilityKey::kFormsClassifications:
      return ModelBasedCapabilityKey::kFormsClassifications;
    case mojom::ModelBasedCapabilityKey::kSummarize:
      return ModelBasedCapabilityKey::kSummarize;
    case mojom::ModelBasedCapabilityKey::kHistoryQueryIntent:
      return ModelBasedCapabilityKey::kHistoryQueryIntent;
    case mojom::ModelBasedCapabilityKey::kBlingPrototyping:
      return ModelBasedCapabilityKey::kBlingPrototyping;
    case mojom::ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return ModelBasedCapabilityKey::kPasswordChangeSubmission;
    case mojom::ModelBasedCapabilityKey::kScamDetection:
      return ModelBasedCapabilityKey::kScamDetection;
    case mojom::ModelBasedCapabilityKey::kPermissionsAi:
      return ModelBasedCapabilityKey::kPermissionsAi;
    case mojom::ModelBasedCapabilityKey::kWritingAssistanceApi:
      return ModelBasedCapabilityKey::kWritingAssistanceApi;
    case mojom::ModelBasedCapabilityKey::kEnhancedCalendar:
      return ModelBasedCapabilityKey::kEnhancedCalendar;
    case mojom::ModelBasedCapabilityKey::kZeroStateSuggestions:
      return ModelBasedCapabilityKey::kZeroStateSuggestions;
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
        MODEL_EXECUTION_FEATURE_FORMS_CLASSIFICATIONS:
      return ModelBasedCapabilityKey::kFormsClassifications;
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
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PERMISSIONS_AI:
      return ModelBasedCapabilityKey::kPermissionsAi;
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PROOFREADER_API:
      return ModelBasedCapabilityKey::kProofreaderApi;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API:
      return ModelBasedCapabilityKey::kWritingAssistanceApi;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_ENHANCED_CALENDAR:
      return ModelBasedCapabilityKey::kEnhancedCalendar;
    case proto::ModelExecutionFeature::
        MODEL_EXECUTION_FEATURE_ZERO_STATE_SUGGESTIONS:
      return ModelBasedCapabilityKey::kZeroStateSuggestions;
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
    case ModelBasedCapabilityKey::kFormsClassifications:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_FORMS_CLASSIFICATIONS;
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
    case ModelBasedCapabilityKey::kPermissionsAi:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_PERMISSIONS_AI;
    case ModelBasedCapabilityKey::kProofreaderApi:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_PROOFREADER_API;
    case ModelBasedCapabilityKey::kWritingAssistanceApi:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API;
    case ModelBasedCapabilityKey::kEnhancedCalendar:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_ENHANCED_CALENDAR;
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_ZERO_STATE_SUGGESTIONS;
  }
}

inline proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    UserVisibleFeatureKey key) {
  return ToModelExecutionFeatureProto(ToModelBasedCapabilityKey(key));
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_
