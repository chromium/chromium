// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_

#include <optional>

#include "base/task/sequenced_task_runner.h"
#include "components/download/public/background_service/download_params.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Provides models configured to be served by the Optimization Guide to be used
// for inference.
class OptimizationGuideModelProvider {
 public:
  // Adds an observer for updates to the model for `optimization_target`.
  // Model loading tasks are expected to be performed on `model_task_runner`.
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
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      OptimizationTargetModelObserver* observer) = 0;

  // Removes an observer for updates to the model for |optimization_target|.
  //
  // If |observer| is registered for multiple targets, |observer| must be
  // removed for all targets that it is added for in order for it to be fully
  // removed from receiving any calls.
  virtual void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) = 0;

  // Sets the scheduling params for a given optimization target. This is
  // optional and only needs to be called if the default download params are not
  // sufficient.
  virtual void SetModelDownloadSchedulingParams(
      proto::OptimizationTarget optimization_target,
      const download::SchedulingParams& params) {}

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
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      OptimizationTargetModelObserver* observer);

  ~OptimizationGuideModelProviderObservation();

  void Observe(proto::OptimizationTarget optimization_target,
               const std::optional<proto::Any>& model_metadata);

  void Reset();

  bool IsRegistered() const;

  OptimizationGuideModelProviderObservation(
      const OptimizationGuideModelProviderObservation&) = delete;
  OptimizationGuideModelProviderObservation& operator=(
      const OptimizationGuideModelProviderObservation&) = delete;

 private:
  const raw_ptr<OptimizationGuideModelProvider> model_provider_;
  const scoped_refptr<base::SequencedTaskRunner> model_task_runner_;
  const raw_ptr<OptimizationTargetModelObserver> observer_;

  // The optimization target for which the models are being observed. If this is
  // OPTIMIZATION_TARGET_UNKNOWN, then the observation is not currently
  // registered.
  proto::OptimizationTarget optimization_target_ =
      proto::OPTIMIZATION_TARGET_UNKNOWN;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
