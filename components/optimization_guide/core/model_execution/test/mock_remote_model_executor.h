// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_REMOTE_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_REMOTE_MODEL_EXECUTOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class MockRemoteModelExecutor : public RemoteModelExecutor {
 public:
  MockRemoteModelExecutor();
  ~MockRemoteModelExecutor() override;

  MOCK_METHOD(void,
              ExecuteModel,
              (ModelBasedCapabilityKey feature,
               const google::protobuf::MessageLite& request_metadata,
               const ModelExecutionOptions& options,
               OptimizationGuideModelExecutionResultCallback callback),
              (override));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_MOCK_REMOTE_MODEL_EXECUTOR_H_
