// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_util.h"

#include <optional>

#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"

namespace optimization_guide {

std::optional<UserVisibleFeatureKey> GetModelExecutionFeature(
    proto::LogAiDataRequest::FeatureCase feature) {
  switch (feature) {
    case proto::LogAiDataRequest::FeatureCase::kCompose:
      return UserVisibleFeatureKey::kCompose;
    case proto::LogAiDataRequest::FeatureCase::kTabOrganization:
      return UserVisibleFeatureKey::kTabOrganization;
    case proto::LogAiDataRequest::FeatureCase::kWallpaperSearch:
      return UserVisibleFeatureKey::kWallpaperSearch;
    case proto::LogAiDataRequest::FeatureCase::kDefault:
      NOTREACHED();
      return std::nullopt;
    case proto::LogAiDataRequest::FeatureCase::FEATURE_NOT_SET:
      // This can be used for testing.
      return std::nullopt;
  }
}

}  // namespace optimization_guide
