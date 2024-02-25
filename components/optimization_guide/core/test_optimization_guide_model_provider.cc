// Copyright 2021 The Chromium Authors
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
    const std::optional<proto::Any>& model_metadata,
    optimization_guide::OptimizationTargetModelObserver* observer) {}

void TestOptimizationGuideModelProvider::
    RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::OptimizationTarget optimization_target,
        optimization_guide::OptimizationTargetModelObserver* observer) {}

}  // namespace optimization_guide
