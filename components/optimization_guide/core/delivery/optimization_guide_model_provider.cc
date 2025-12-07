// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"

namespace optimization_guide {

OptimizationGuideModelProviderObservation::
    OptimizationGuideModelProviderObservation(
        OptimizationGuideModelProvider* model_provider,
        scoped_refptr<base::SequencedTaskRunner> model_task_runner,
        OptimizationTargetModelObserver* observer)
    : model_provider_(model_provider),
      model_task_runner_(model_task_runner),
      observer_(observer) {}

OptimizationGuideModelProviderObservation::
    ~OptimizationGuideModelProviderObservation() {
  Reset();
}

void OptimizationGuideModelProviderObservation::Observe(
    proto::OptimizationTarget optimization_target,
    const std::optional<proto::Any>& model_metadata) {
  optimization_target_ = optimization_target;
  model_provider_->AddObserverForOptimizationTargetModel(
      optimization_target, model_metadata, model_task_runner_, observer_);
}

void OptimizationGuideModelProviderObservation::Reset() {
  if (optimization_target_ != proto::OPTIMIZATION_TARGET_UNKNOWN) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_target_, observer_);
    optimization_target_ = proto::OPTIMIZATION_TARGET_UNKNOWN;
  }
}

bool OptimizationGuideModelProviderObservation::IsRegistered() const {
  return optimization_target_ != proto::OPTIMIZATION_TARGET_UNKNOWN;
}

}  // namespace optimization_guide
