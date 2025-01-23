// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_UTIL_H_

#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

class PrefService;
namespace optimization_guide {

// Returns the hashed client id with the feature and day.
int64_t GetHashedModelQualityClientId(
    proto::LogAiDataRequest::FeatureCase feature,
    base::Time day,
    int64_t client_id);

// Creates a new client id if not persisted to prefs. Returns a different ID for
// different `feature` for each day.
int64_t GetOrCreateModelQualityClientId(
    proto::LogAiDataRequest::FeatureCase feature,
    PrefService* pref_service);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_UTIL_H_
