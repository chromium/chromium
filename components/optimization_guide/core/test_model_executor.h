// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_

#include "components/optimization_guide/core/model_executor.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace optimization_guide {

class TestModelExecutor
    : public ModelExecutor<std::vector<float>, const std::vector<float>&> {
 public:
  TestModelExecutor() = default;
  ~TestModelExecutor() override = default;

  void InitializeAndMoveToBackgroundThread(
      proto::OptimizationTarget,
      scoped_refptr<base::SequencedTaskRunner>,
      scoped_refptr<base::SequencedTaskRunner>) override {}

  void UpdateModelFile(const base::FilePath&) override {}

  void UnloadModel() override {}

  void SetShouldUnloadModelOnComplete(bool should_auto_unload) override {}

  using ExecutionCallback =
      base::OnceCallback<void(const absl::optional<std::vector<float>>&)>;
  void SendForExecution(ExecutionCallback ui_callback_on_complete,
                        base::TimeTicks start_time,
                        const std::vector<float>& args) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_
