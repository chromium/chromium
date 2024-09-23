// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_OPTIMIZATION_TARGET_SEGMENTATION_DUMMY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_OPTIMIZATION_TARGET_SEGMENTATION_DUMMY_H_

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

// Feature flag for enabling OptimizationTargetSegmentationDummy segment.
BASE_DECLARE_FEATURE(kSegmentationPlatformOptimizationTargetSegmentationDummy);

// Model to predict whether the user belongs to
// OptimizationTargetSegmentationDummy segment.
class OptimizationTargetSegmentationDummy : public DefaultModelProvider {
 public:
  OptimizationTargetSegmentationDummy();
  ~OptimizationTargetSegmentationDummy() override = default;

  OptimizationTargetSegmentationDummy(
      const OptimizationTargetSegmentationDummy&) = delete;
  OptimizationTargetSegmentationDummy& operator=(
      const OptimizationTargetSegmentationDummy&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_DEFAULT_MODEL_OPTIMIZATION_TARGET_SEGMENTATION_DUMMY_H_
