// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_

#include <optional>

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
};

// A "real" feature implemented by a model-based capability.
// These will have their own prefs / settings / policies etc.
enum class UserVisibleFeatureKey {
  kCompose = static_cast<int>(ModelBasedCapabilityKey::kCompose),
  kTabOrganization =
      static_cast<int>(ModelBasedCapabilityKey::kTabOrganization),
  kWallpaperSearch =
      static_cast<int>(ModelBasedCapabilityKey::kWallpaperSearch),
};

inline constexpr std::array<UserVisibleFeatureKey, 3>
    kAllUserVisibleFeatureKeys = {
        UserVisibleFeatureKey::kCompose,
        UserVisibleFeatureKey::kTabOrganization,
        UserVisibleFeatureKey::kWallpaperSearch,
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
  }
}

inline proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    UserVisibleFeatureKey key) {
  return ToModelExecutionFeatureProto(ToModelBasedCapabilityKey(key));
}

// TODO: crbug.com/331306557 - Cleanup after migration, if possible.
inline std::optional<ModelBasedCapabilityKey> ToModelBasedCapabilityKey(
    proto::ModelExecutionFeature key) {
  switch (key) {
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
    default:
      return std::nullopt;
  }
}

// TODO: crbug.com/331306557 - Cleanup after migration, if possible.
inline std::optional<UserVisibleFeatureKey> ToUserVisibleFeatureKey(
    ModelBasedCapabilityKey key) {
  switch (key) {
    case ModelBasedCapabilityKey::kCompose:
      return UserVisibleFeatureKey::kCompose;
    case ModelBasedCapabilityKey::kTabOrganization:
      return UserVisibleFeatureKey::kTabOrganization;
    case ModelBasedCapabilityKey::kWallpaperSearch:
      return UserVisibleFeatureKey::kWallpaperSearch;
    case ModelBasedCapabilityKey::kTest:
    case ModelBasedCapabilityKey::kTextSafety:
      return std::nullopt;
  }
}

// TODO: crbug.com/331306557 - Cleanup after migration.
inline std::optional<UserVisibleFeatureKey> ToUserVisibleFeatureKey(
    proto::ModelExecutionFeature key) {
  auto key2 = ToModelBasedCapabilityKey(key);
  return key2 ? ToUserVisibleFeatureKey(*key2) : std::nullopt;
}

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_FEATURE_KEYS_H_
