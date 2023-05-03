// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

// The key service holds on device tail model executor and its model observer.
class OnDeviceTailModelService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using ResultCallback = base::OnceCallback<void(
      std::vector<OnDeviceTailModelExecutor::Prediction>)>;

  // TODO(crbug.com/1372112): move this struct into model executor class.
  struct OnDeviceTailModelInput {
    std::string sanitized_input;
    std::string previous_query;
    size_t max_num_suggestions;
    size_t max_num_step;
    float probability_threshold;
  };

  explicit OnDeviceTailModelService(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~OnDeviceTailModelService() override;

  // Disallow copy/assign.
  OnDeviceTailModelService(const OnDeviceTailModelService&) = delete;
  OnDeviceTailModelService& operator=(const OnDeviceTailModelService&) = delete;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

  // Calls the model executor to generate predictions for the input.
  void GetPredictionsForInput(const OnDeviceTailModelInput& input,
                              ResultCallback result_callback);

 private:
  friend class OnDeviceTailModelServiceTest;

  // The task runner to run tail model executor.
  scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner_;

  using ExecutorUniquePtr =
      std::unique_ptr<OnDeviceTailModelExecutor, base::OnTaskRunnerDeleter>;

  // The executor to run the tail suggest model.
  ExecutorUniquePtr tail_model_executor_;

  // Optimization Guide Service that provides model files for this service.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_ =
      nullptr;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_SERVICE_H_
