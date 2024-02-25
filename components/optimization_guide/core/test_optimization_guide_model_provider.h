// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_

#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace optimization_guide {

// An implementation of |OptimizationGuideModelProvider| that can be selectively
// mocked out for unit testing features that rely on the Optimization Guide in
// //components/...
class TestOptimizationGuideModelProvider
    : public OptimizationGuideModelProvider {
 public:
  TestOptimizationGuideModelProvider();
  ~TestOptimizationGuideModelProvider() override;
  TestOptimizationGuideModelProvider(
      const TestOptimizationGuideModelProvider&) = delete;
  TestOptimizationGuideModelProvider& operator=(
      const TestOptimizationGuideModelProvider&) = delete;

  // OptimizationGuideModelProvider implementation:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_OPTIMIZATION_GUIDE_MODEL_PROVIDER_H_
