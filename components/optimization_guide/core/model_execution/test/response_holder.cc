// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/response_holder.h"

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

std::string GetComposeOutput(const StreamingResponse& response) {
  return ParsedAnyMetadata<proto::ComposeResponse>(response.response)->output();
}

}  // namespace

RemoteResponseHolder::RemoteResponseHolder() = default;
RemoteResponseHolder::~RemoteResponseHolder() = default;

OptimizationGuideModelExecutionResultCallback
RemoteResponseHolder::GetCallback() {
  CHECK(!weak_ptr_factory_.HasWeakPtrs());  // Shouldn't be reused.
  return base::BindRepeating(&RemoteResponseHolder::OnResponse,
                             weak_ptr_factory_.GetWeakPtr());
}

void RemoteResponseHolder::OnResponse(
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  log_entry_ = std::move(log_entry);
  result_.emplace(std::move(result));
  future_.SetValue(result_->response.has_value());
}

ResponseHolder::ResponseHolder() : weak_ptr_factory_(this) {}
ResponseHolder::~ResponseHolder() = default;

OptimizationGuideModelExecutionResultStreamingCallback
ResponseHolder::GetStreamingCallback() {
  CHECK(!weak_ptr_factory_.HasWeakPtrs());  // Shouldn't be reused.
  return base::BindRepeating(&ResponseHolder::OnStreamingResponse,
                             weak_ptr_factory_.GetWeakPtr());
}

void ResponseHolder::OnStreamingResponse(
    OptimizationGuideModelStreamingExecutionResult result) {
  if (result.response.has_value() && !result.response->is_complete) {
    EXPECT_TRUE(result.provided_by_on_device);
    partial_responses_.push_back(GetComposeOutput(*result.response));
    return;
  }
  provided_by_on_device_ = result.provided_by_on_device;
  model_execution_info_received_ = std::move(result.execution_info);
  if (model_execution_info_received_) {
    auto& id = model_execution_info_received_->execution_id();
    if (result.provided_by_on_device) {
      EXPECT_TRUE(base::StartsWith(id, "on-device"));
    } else if (result.response.has_value()) {
      EXPECT_FALSE(id.empty());
    } else {
      // May be empty in some server error cases.
    }
  }
  if (!result.response.has_value()) {
    response_error_ = result.response.error().error();
    final_status_future_.SetValue(false);
    return;
  }
  input_token_count_ = result.response->input_token_count;
  output_token_count_ = result.response->output_token_count;
  std::string full_response;
  for (auto partial_response : partial_responses_) {
    full_response += partial_response;
  }
  full_response += GetComposeOutput(*result.response);
  response_received_ = full_response;
  final_status_future_.SetValue(true);
}

}  // namespace optimization_guide
