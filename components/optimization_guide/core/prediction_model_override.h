// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_

#include "base/functional/callback.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {

// Attempts to parse the result from |GetModelOverrideForOptimizationTarget|
// into a |proto::PredictionModel|, returning the result in the given callback
// or nullptr if there was an error. In the event of an error, check LOG(ERROR).
// Returns true if an override was present for the named target and false
// otherwise.
using OnPredictionModelBuiltCallback =
    base::OnceCallback<void(std::unique_ptr<proto::PredictionModel>)>;
bool BuildPredictionModelFromCommandLineForOptimizationTarget(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir,
    OnPredictionModelBuiltCallback callback);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_OVERRIDE_H_
