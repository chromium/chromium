// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_REMOTE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_REMOTE_H_

#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/execute_remote_fn.h"

namespace optimization_guide {

ExecuteRemoteFn FailOnRemoteFallback();

class ExpectedRemoteFallback final {
 public:
  struct FallbackArgs {
    FallbackArgs();
    FallbackArgs(FallbackArgs&&);
    ~FallbackArgs();

    FallbackArgs& operator=(FallbackArgs&&);

    ModelBasedCapabilityKey feature;
    std::unique_ptr<google::protobuf::MessageLite> request;
    std::optional<base::TimeDelta> timeout;
    std::unique_ptr<proto::LogAiDataRequest> log;
    OptimizationGuideModelExecutionResultCallback callback;

    const auto& logged_executions() {
      return log->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
    }
  };

  ExpectedRemoteFallback();
  ~ExpectedRemoteFallback();

  ExecuteRemoteFn CreateExecuteRemoteFn();

  FallbackArgs Take() { return future_.Take(); }

 private:
  base::test::TestFuture<FallbackArgs> future_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_REMOTE_H_
