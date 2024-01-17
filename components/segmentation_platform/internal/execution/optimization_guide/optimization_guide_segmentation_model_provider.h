// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SEGMENTATION_MODEL_PROVIDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SEGMENTATION_MODEL_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"


namespace segmentation_platform {

class OptimizationGuideSegmentationModelHandler;

// Model provider implementation that uses optimization guide to fetch and
// execute models.
class OptimizationGuideSegmentationModelProvider : public ModelProvider {
 public:
  OptimizationGuideSegmentationModelProvider(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      proto::SegmentId segment_id);

  ~OptimizationGuideSegmentationModelProvider() override;

  // Disallow copy/assign.
  OptimizationGuideSegmentationModelProvider(
      const OptimizationGuideSegmentationModelProvider&) = delete;
  OptimizationGuideSegmentationModelProvider& operator=(
      const OptimizationGuideSegmentationModelProvider&) = delete;

  // ModelProvider impl:
  void InitAndFetchModel(
      const ModelUpdatedCallback& model_updated_callback) override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
  bool ModelAvailable() override;

  OptimizationGuideSegmentationModelHandler& model_handler_for_testing() {
    return *model_handler_;
  }

 private:
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  std::unique_ptr<OptimizationGuideSegmentationModelHandler> model_handler_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SEGMENTATION_MODEL_PROVIDER_H_
