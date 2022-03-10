// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/dummy_model_execution_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
using proto::OptimizationTarget;
}  // namespace optimization_guide

namespace segmentation_platform {
namespace {
void RunModelExecutionCallback(
    ModelExecutionManager::ModelExecutionCallback callback) {
  std::move(callback).Run(
      std::make_pair(0, ModelExecutionStatus::kExecutionError));
}
}  // namespace

DummyModelExecutionManager::DummyModelExecutionManager() = default;

DummyModelExecutionManager::~DummyModelExecutionManager() = default;

void DummyModelExecutionManager::ExecuteModel(
    const proto::SegmentInfo& segment_info,
    ModelExecutionCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunModelExecutionCallback, std::move(callback)));
  return;
}

}  // namespace segmentation_platform
