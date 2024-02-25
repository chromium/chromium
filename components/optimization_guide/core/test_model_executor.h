// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_executor.h"

namespace optimization_guide {

class TestModelExecutor
    : public ModelExecutor<std::vector<float>, const std::vector<float>&> {
 public:
  TestModelExecutor() = default;
  ~TestModelExecutor() override = default;

  void InitializeAndMoveToExecutionThread(
      std::optional<base::TimeDelta>,
      proto::OptimizationTarget,
      scoped_refptr<base::SequencedTaskRunner>,
      scoped_refptr<base::SequencedTaskRunner>) override {}

  void UpdateModelFile(base::optional_ref<const base::FilePath>) override {}

  void UnloadModel() override {}

  void SetShouldUnloadModelOnComplete(bool should_auto_unload) override {}

  void SetShouldPreloadModel(bool should_preload_model) override {}

  using ExecutionCallback =
      base::OnceCallback<void(const std::optional<std::vector<float>>&)>;
  void SendForExecution(ExecutionCallback callback_on_complete,
                        base::TimeTicks start_time,
                        const std::vector<float>& args) override;

  using BatchExecutionCallback = base::OnceCallback<void(
      const std::vector<std::optional<std::vector<float>>>&)>;
  void SendForBatchExecution(
      BatchExecutionCallback callback_on_complete,
      base::TimeTicks start_time,
      const std::vector<std::vector<float>>& args) override;

  std::vector<std::optional<std::vector<float>>> SendForBatchExecutionSync(
      const std::vector<std::vector<float>>& args) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_
