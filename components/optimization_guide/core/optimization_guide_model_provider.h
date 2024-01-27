// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_

#include <optional>

#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Provides models configured to be served by the Optimization Guide to be used
// for inference.
class OptimizationGuideModelProvider {
 public:
  // Adds an observer for updates to the model for |optimization_target|.
  //
  // It is assumed that any model retrieved this way will be passed to the
  // Machine Learning Service for inference.
  //
  // It is also assumed that there will only be one observer per optimization
  // target, so if multiple observers are registered, this will crash in debug
  // builds and be a no-op in release builds.
  virtual void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) = 0;

  // Removes an observer for updates to the model for |optimization_target|.
  //
  // If |observer| is registered for multiple targets, |observer| must be
  // removed for all targets that it is added for in order for it to be fully
  // removed from receiving any calls.
  virtual void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) = 0;

 protected:
  OptimizationGuideModelProvider() = default;
  virtual ~OptimizationGuideModelProvider() = default;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
