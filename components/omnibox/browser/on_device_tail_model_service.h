// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
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

  explicit OnDeviceTailModelService(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~OnDeviceTailModelService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Disallow copy/assign.
  OnDeviceTailModelService(const OnDeviceTailModelService&) = delete;
  OnDeviceTailModelService& operator=(const OnDeviceTailModelService&) = delete;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // Calls the model executor to generate predictions for the input.
  void GetPredictionsForInput(
      const OnDeviceTailModelExecutor::ModelInput& input,
      ResultCallback result_callback);

  // Helper which unloads the executor from memory when memory pressure is high.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

 private:
  friend class OnDeviceTailModelServiceTest;
  friend class FakeOnDeviceTailModelService;

  // The default constructor used with tests only, which will create nullptrs
  // for all private members such that tests can initialize members later on
  // demand.
  OnDeviceTailModelService();

  // The task runner to run tail model executor.
  scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner_ =
      nullptr;

  using ExecutorUniquePtr =
      std::unique_ptr<OnDeviceTailModelExecutor, base::OnTaskRunnerDeleter>;

  // The executor to run the tail suggest model.
  ExecutorUniquePtr tail_model_executor_;

  // Optimization Guide Service that provides model files for this service.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_ =
      nullptr;

  // The memory pressure listener which unloads executor when memory pressure
  // level is high.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  base::WeakPtrFactory<OnDeviceTailModelService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_TAIL_MODEL_SERVICE_H_
