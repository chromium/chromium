// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"

namespace optimization_guide {

TestOptimizationGuideModelProvider::TestOptimizationGuideModelProvider() =
    default;
TestOptimizationGuideModelProvider::~TestOptimizationGuideModelProvider() =
    default;

void TestOptimizationGuideModelProvider::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const absl::optional<proto::Any>& model_metadata,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  registered_observers_for_optimization_targets_[optimization_target]
      .AddObserver(observer);
}

void TestOptimizationGuideModelProvider::
    RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::OptimizationTarget optimization_target,
        optimization_guide::OptimizationTargetModelObserver* observer) {
  auto observers_it =
      registered_observers_for_optimization_targets_.find(optimization_target);
  if (observers_it == registered_observers_for_optimization_targets_.end())
    return;

  observers_it->second.RemoveObserver(observer);
}

void TestOptimizationGuideModelProvider::NotifyModelFileUpdate(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& model_file_path) {
  for (auto& observer :
       registered_observers_for_optimization_targets_[optimization_target]) {
    observer.OnModelFileUpdated(proto::OPTIMIZATION_TARGET_MODEL_VALIDATION,
                                /*model_metadata=*/absl::nullopt,
                                model_file_path);
  }
}

}  // namespace optimization_guide
