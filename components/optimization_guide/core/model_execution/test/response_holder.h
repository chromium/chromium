// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_RESPONSE_HOLDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_RESPONSE_HOLDER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"

namespace optimization_guide {

class ResponseHolder {
 public:
  ResponseHolder();
  ~ResponseHolder();

  OptimizationGuideModelExecutionResultStreamingCallback GetStreamingCallback();

  // Wait for and get the final execution status (true if completed without
  // error).
  bool GetFinalStatus() { return final_status_future_.Get(); }

  const std::optional<std::string>& value() const { return response_received_; }
  const std::vector<std::string>& partials() const {
    return partial_responses_;
  }
  const std::optional<OnDeviceError>& error() const { return response_error_; }
  const std::optional<bool>& provided_by_on_device() const {
    return provided_by_on_device_;
  }
  proto::ModelExecutionInfo* model_execution_info() {
    return model_execution_info_received_.get();
  }

  size_t input_token_count() const { return input_token_count_; }

  size_t output_token_count() const { return output_token_count_; }

 private:
  void OnStreamingResponse(
      OptimizationGuideModelStreamingExecutionResult result);

  base::test::TestFuture<bool> final_status_future_;
  std::vector<std::string> partial_responses_;
  std::optional<std::string> response_received_;
  std::optional<bool> provided_by_on_device_;
  std::unique_ptr<proto::ModelExecutionInfo> model_execution_info_received_;
  std::optional<OnDeviceError> response_error_;
  size_t input_token_count_ = 0;
  size_t output_token_count_ = 0;
  base::WeakPtrFactory<ResponseHolder> weak_ptr_factory_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_RESPONSE_HOLDER_H_
