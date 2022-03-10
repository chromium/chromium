// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DUMMY_MODEL_EXECUTION_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DUMMY_MODEL_EXECUTION_MANAGER_H_

#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"

namespace segmentation_platform {

// The DummyModelExecutionManager provides an implementation of the core
// ModelExecutionManager that always posts a callback results with
// ModelExecutionStatus::kExecutionError.
//
// It has no dependencies on TFLite, so it can be used even when
// BUILDFLAG(BUILD_WITH_TFLITE_LIB) is not set.
class DummyModelExecutionManager : public ModelExecutionManager {
 public:
  DummyModelExecutionManager();
  ~DummyModelExecutionManager() override;

  // Disallow copy/assign.
  DummyModelExecutionManager(const DummyModelExecutionManager&) = delete;
  DummyModelExecutionManager& operator=(const DummyModelExecutionManager&) =
      delete;

  // ModelExecutionManager overrides.
  void ExecuteModel(const proto::SegmentInfo& segment_info,
                    ModelExecutionCallback callback) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DUMMY_MODEL_EXECUTION_MANAGER_H_
