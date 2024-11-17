// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/response_holder.h"

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

std::string GetOutput(const StreamingResponse& response) {
  return ParsedAnyMetadata<proto::ComposeResponse>(response.response)->output();
}

}  // namespace

ResponseHolder::ResponseHolder() : weak_ptr_factory_(this) {}
ResponseHolder::~ResponseHolder() = default;

OptimizationGuideModelExecutionResultCallback ResponseHolder::GetCallback() {
  Clear();
  return base::BindRepeating(&ResponseHolder::OnResponse,
                             weak_ptr_factory_.GetWeakPtr());
}

OptimizationGuideModelExecutionResultStreamingCallback
ResponseHolder::GetStreamingCallback() {
  Clear();
  return base::BindRepeating(&ResponseHolder::OnStreamingResponse,
                             weak_ptr_factory_.GetWeakPtr());
}

void ResponseHolder::Clear() {
  final_status_future_.Clear();
  log_entry_received_.reset();
  model_execution_info_received_.reset();
  streamed_responses_.clear();
  response_error_ = std::nullopt;
  provided_by_on_device_ = std::nullopt;
  response_received_ = std::nullopt;
}

void ResponseHolder::OnResponse(
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  if (result.response.has_value()) {
    OnStreamingResponse(OptimizationGuideModelStreamingExecutionResult(
        base::ok(StreamingResponse{.response = result.response.value(),
                                   .is_complete = true}),
        /*provided_by_on_device=*/false, std::move(log_entry),
        std::move(result.execution_info)));
  } else {
    OnStreamingResponse(OptimizationGuideModelStreamingExecutionResult(
        base::unexpected(result.response.error()),
        /*provided_by_on_device=*/false, std::move(log_entry),
        std::move(result.execution_info)));
  }
}

void ResponseHolder::OnStreamingResponse(
    OptimizationGuideModelStreamingExecutionResult result) {
  if (result.response.has_value() && !result.response->is_complete) {
    EXPECT_FALSE(result.log_entry);
    EXPECT_TRUE(result.provided_by_on_device);
    streamed_responses_.push_back(GetOutput(*result.response));
    return;
  }
  provided_by_on_device_ = result.provided_by_on_device;
  log_entry_received_ = std::move(result.log_entry);
  model_execution_info_received_ = std::move(result.execution_info);
  if (log_entry_received_) {
    auto& id = log_entry_received_->log_ai_data_request()
                   ->model_execution_info()
                   .execution_id();
    if (result.provided_by_on_device) {
      EXPECT_TRUE(base::StartsWith(id, "on-device"));
    } else if (result.response.has_value()) {
      EXPECT_FALSE(id.empty());
    } else {
      // May be empty in some server error cases.
    }
  }
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
  response_received_ = GetOutput(*result.response);
  final_status_future_.SetValue(true);
}

}  // namespace optimization_guide
