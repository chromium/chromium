// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_util.h"

#include "base/notreached.h"

namespace optimization_guide {

proto::ModelExecutionFeature GetModelExecutionFeature(
    proto::LogAiDataRequest::FeatureCase feature) {
  switch (feature) {
    case proto::LogAiDataRequest::FeatureCase::kCompose:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE;
    case proto::LogAiDataRequest::FeatureCase::kTabOrganization:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION;
    case proto::LogAiDataRequest::FeatureCase::kWallpaperSearch:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH;
    case proto::LogAiDataRequest::FeatureCase::kDefault:
      NOTREACHED();
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED;
    case proto::LogAiDataRequest::FeatureCase::FEATURE_NOT_SET:
      // This can be used for testing.
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED;
  }
}

}  // namespace optimization_guide
