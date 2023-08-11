// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fake_on_device_tail_model_service.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

FakeOnDeviceTailModelService::FakeOnDeviceTailModelService() = default;

void FakeOnDeviceTailModelService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  if (optimization_target !=
      optimization_guide::proto::
          OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST) {
    return;
  }

  // Only initialize the runner and the executor here, such that tests without
  // test task environment will not fail unless they explicitly call
  // `OnModelUpdated`.
  model_executor_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  tail_model_executor_ =
      ExecutorUniquePtr(new OnDeviceTailModelExecutor(),
                        base::OnTaskRunnerDeleter(model_executor_task_runner_));

  OnDeviceTailModelService::OnModelUpdated(optimization_target, model_info);
}
