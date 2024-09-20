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

ResponseHolder::ResponseHolder() : weak_ptr_factory_(this) {}
ResponseHolder::~ResponseHolder() = default;

OptimizationGuideModelExecutionResultStreamingCallback
ResponseHolder::callback() {
  final_status_future_.Clear();
  return base::BindRepeating(&ResponseHolder::OnResponse,
                             weak_ptr_factory_.GetWeakPtr());
}

void ResponseHolder::OnResponse(
    OptimizationGuideModelStreamingExecutionResult result) {
  log_entry_received_ = std::move(result.log_entry);
  if (log_entry_received_) {
    // Make sure that an execution ID is always generated if we return a log
    // entry.
    ASSERT_FALSE(log_entry_received_->log_ai_data_request()
                     ->model_execution_info()
                     .execution_id()
                     .empty());
    EXPECT_TRUE(base::StartsWith(log_entry_received_->log_ai_data_request()
                                     ->model_execution_info()
                                     .execution_id(),
                                 "on-device"));
  }
  if (!result.response.has_value()) {
    response_error_ = result.response.error().error();
    final_status_future_.SetValue(false);
    return;
  }
  provided_by_on_device_ = result.provided_by_on_device;
  auto response =
      ParsedAnyMetadata<proto::ComposeResponse>(result.response->response);
  if (result.response->is_complete) {
    response_received_ = response->output();
    final_status_future_.SetValue(true);
  } else {
    streamed_responses_.push_back(response->output());
  }
}

}  // namespace optimization_guide
