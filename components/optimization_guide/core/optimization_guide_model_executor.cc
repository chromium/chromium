// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_model_executor.h"

#include "base/memory/ptr_util.h"

namespace optimization_guide {

OptimizationGuideModelExecutor::OptimizationGuideModelExecutor() = default;
OptimizationGuideModelExecutor::~OptimizationGuideModelExecutor() = default;

OptimizationGuideModelExecutor::Session::Session() = default;
OptimizationGuideModelExecutor::Session::~Session() = default;

std::unique_ptr<google::protobuf::MessageLite>
OptimizationGuideModelExecutor::Session::MergeContext(
    const google::protobuf::MessageLite& request) {
  // Create a message of the correct type.
  auto message = base::WrapUnique(request.New());
  // First merge in the current context.
  if (context_) {
    message->CheckTypeAndMergeFrom(*context_);
  }
  // Then merge in the request.
  message->CheckTypeAndMergeFrom(request);
  return message;
}

}  // namespace optimization_guide
