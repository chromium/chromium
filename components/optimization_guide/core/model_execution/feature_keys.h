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
  kHistorySearch =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_HISTORY_SEARCH,
  kFormsClassifications = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_FORMS_CLASSIFICATIONS,
  kBlingPrototyping =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_BLING_PROTOTYPING,
  kPasswordChangeSubmission = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_PASSWORD_CHANGE_SUBMISSION,
  kEnhancedCalendar =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_ENHANCED_CALENDAR,
  kZeroStateSuggestions = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_ZERO_STATE_SUGGESTIONS,
  kWalletablePassExtraction = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_WALLETABLE_PASS_EXTRACTION,
  kAmountExtraction =
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_AMOUNT_EXTRACTION,
  kIosSmartTabGrouping = proto::ModelExecutionFeature::
      MODEL_EXECUTION_FEATURE_IOS_SMART_TAB_GROUPING,
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
    case ModelBasedCapabilityKey::kHistorySearch:
      return out << "HistorySearch";
    case ModelBasedCapabilityKey::kFormsClassifications:
      return out << "FormsClassifications";
    case ModelBasedCapabilityKey::kBlingPrototyping:
      return out << "BlingPrototyping";
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return out << "PasswordChangeSubmission";
    case ModelBasedCapabilityKey::kEnhancedCalendar:
      return out << "EnhancedCalendar";
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return out << "ZeroStateSuggestions";
    case ModelBasedCapabilityKey::kWalletablePassExtraction:
      return out << "WalletablePassExtraction";
    case ModelBasedCapabilityKey::kAmountExtraction:
      return out << "AmountExtraction";
    case ModelBasedCapabilityKey::kIosSmartTabGrouping:
      return out << "IosSmartTabGrouping";
  }
  return out;
}

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
    case ModelBasedCapabilityKey::kHistorySearch:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_HISTORY_SEARCH;
    case ModelBasedCapabilityKey::kFormsClassifications:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_FORMS_CLASSIFICATIONS;
    case ModelBasedCapabilityKey::kBlingPrototyping:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_BLING_PROTOTYPING;
    case ModelBasedCapabilityKey::kPasswordChangeSubmission:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_PASSWORD_CHANGE_SUBMISSION;
    case ModelBasedCapabilityKey::kEnhancedCalendar:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_ENHANCED_CALENDAR;
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_ZERO_STATE_SUGGESTIONS;
    case ModelBasedCapabilityKey::kWalletablePassExtraction:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WALLETABLE_PASS_EXTRACTION;
    case ModelBasedCapabilityKey::kAmountExtraction:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_AMOUNT_EXTRACTION;
    case ModelBasedCapabilityKey::kIosSmartTabGrouping:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_IOS_SMART_TAB_GROUPING;
  }
}

inline proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    UserVisibleFeatureKey key) {
  return ToModelExecutionFeatureProto(ToModelBasedCapabilityKey(key));
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_
