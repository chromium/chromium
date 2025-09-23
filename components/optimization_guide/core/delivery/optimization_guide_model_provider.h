// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_

#include <optional>

#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
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

// Helper class to facilitate observing model updates for a given optimization
// target. This is similar to base::ScopedObservation and takes care of adding
// and removing the observer from the model provider.
class OptimizationGuideModelProviderObservation {
 public:
  OptimizationGuideModelProviderObservation(
      OptimizationGuideModelProvider* model_provider,
      OptimizationTargetModelObserver* observer)
      : model_provider_(model_provider), observer_(observer) {}

  ~OptimizationGuideModelProviderObservation() { Reset(); }

  void Observe(proto::OptimizationTarget optimization_target,
               const std::optional<proto::Any>& model_metadata) {
    optimization_target_ = optimization_target;
    model_provider_->AddObserverForOptimizationTargetModel(
        optimization_target, model_metadata, observer_);
  }

  void Reset() {
    if (optimization_target_ != proto::OPTIMIZATION_TARGET_UNKNOWN) {
      model_provider_->RemoveObserverForOptimizationTargetModel(
          optimization_target_, observer_);
      optimization_target_ = proto::OPTIMIZATION_TARGET_UNKNOWN;
    }
  }

  bool IsRegistered() const {
    return optimization_target_ != proto::OPTIMIZATION_TARGET_UNKNOWN;
  }

  OptimizationGuideModelProviderObservation(
      const OptimizationGuideModelProviderObservation&) = delete;
  OptimizationGuideModelProviderObservation& operator=(
      const OptimizationGuideModelProviderObservation&) = delete;

 private:
  const raw_ptr<OptimizationGuideModelProvider> model_provider_;
  const raw_ptr<OptimizationTargetModelObserver> observer_;

  // The optimization target for which the models are being observed. If this is
  // OPTIMIZATION_TARGET_UNKNOWN, then the observation is not currently
  // registered.
  proto::OptimizationTarget optimization_target_ =
      proto::OPTIMIZATION_TARGET_UNKNOWN;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
