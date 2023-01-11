// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTOR_H_

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// This class handles the execution, loading, unloading, and associated metrics
// of machine learning models in Optimization Guide on a specified thread. This
// class is meant to be used and owned by an instance of |ModelHandler|. A
// ModelExecutor must be passed to a ModelHandler's constructor, this design
// allows the implementer of a ModelExecutor to define how the model is built
// and executed. See also tflite_model_executor.h, base_model_executor.h, and
// base_model_executor_helpers.h in this directory for helpful derived classes.
//
// Lifetime: This class can be constructed on any thread but cannot do anything
// useful until |InitializeAndMoveToExecutionThread| is called. After that
// method is called, all subsequent calls to this class must be made through the
// |execution_task_runner| that was passed to initialize. Furthermore, all
// WeakPointers of this class must only be dereferenced on the
// |execution_task_runner| thread as well. This in turn means that this class
// must be destroyed on the |execution_task_runner| thread as well.
template <class OutputType, class... InputTypes>
class ModelExecutor {
 public:
  ModelExecutor() = default;
  virtual ~ModelExecutor() = default;

  // If |model_inference_timeout| is nullopt a default value will be used,
  // controlled by the optimization guide.
  virtual void InitializeAndMoveToExecutionThread(
      absl::optional<base::TimeDelta> model_inference_timeout,
      proto::OptimizationTarget optimization_target,
      scoped_refptr<base::SequencedTaskRunner> execution_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner) = 0;

  // Updates model file. If `SetShouldUnloadModelOnComplete` is false,
  // immedidately loads model into memory.
  virtual void UpdateModelFile(const base::FilePath& file_path) = 0;

  virtual void UnloadModel() = 0;

  // Sets whether the model file should be unloaded from memory after each
  // execution. If set to false, use |UnloadModel| to unload the model when
  // needed.
  virtual void SetShouldUnloadModelOnComplete(bool should_auto_unload) = 0;

  using ExecutionCallback =
      base::OnceCallback<void(const absl::optional<OutputType>&)>;
  virtual void SendForExecution(ExecutionCallback callback_on_complete,
                                base::TimeTicks start_time,
                                InputTypes... args) = 0;

  // IMPORTANT: These WeakPointers must only be dereferenced on the
  // |execution_task_runner| thread.
  base::WeakPtr<ModelExecutor> GetWeakPtrForExecutionThread() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  ModelExecutor(const ModelExecutor&) = delete;
  ModelExecutor& operator=(const ModelExecutor&) = delete;

 private:
  base::WeakPtrFactory<ModelExecutor> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTOR_H_
