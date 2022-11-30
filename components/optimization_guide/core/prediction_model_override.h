// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_

#include "base/callback.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Attempts to parse the result from |GetModelOverrideForOptimizationTarget|
// into a |proto::PredictionModel|, returning the result in the given callback
// or nullptr if there was an error. In the event of an error, check LOG(ERROR).
using OnPredictionModelBuiltCallback =
    base::OnceCallback<void(std::unique_ptr<proto::PredictionModel>)>;
void BuildPredictionModelFromCommandLineForOptimizationTarget(
    proto::OptimizationTarget optimization_target,
    OnPredictionModelBuiltCallback callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_