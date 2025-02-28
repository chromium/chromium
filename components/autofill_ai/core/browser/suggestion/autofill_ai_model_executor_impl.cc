// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor_impl.h"

#include <cmath>
#include <optional>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace autofill_ai {

AutofillAiModelExecutorImpl::AutofillAiModelExecutorImpl(
    optimization_guide::OptimizationGuideModelExecutor* model_executor,
    optimization_guide::ModelQualityLogsUploaderService* logs_uploader)
    : model_executor_(CHECK_DEREF(model_executor)) {
  // TODO(crbug.com/389631477): Remove logging until we have a need for it.
  if (logs_uploader) {
    logs_uploader_ = logs_uploader->GetWeakPtr();
  }
}

AutofillAiModelExecutorImpl::~AutofillAiModelExecutorImpl() = default;

void AutofillAiModelExecutorImpl::GetPredictions(
    autofill::FormData form_data,
    base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map,
    base::flat_map<autofill::FieldGlobalId, bool> field_sensitivity_map,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    PredictionsReceivedCallback callback) {
  // Construct request.
  optimization_guide::proto::AutofillAiTypeRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  if (kSendTitleURL.Get()) {
    page_context->set_url(form_data.url().spec());
    page_context->set_title(ax_tree_update.tree_data().title());
  } else {
    page_context->set_url(form_data.main_frame_origin().Serialize());
  }
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update);

  // TODO(crbug.com/389631477): Remove eligibility and sensitivity.
  *request.mutable_form_data() =
      ToFormDataProto(form_data, field_eligibility_map, field_sensitivity_map);

  SetLatestRequestForDebugging(request);
  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::FormsClassificationsLoggingData>
      wrapper_callback =
          base::BindOnce(&AutofillAiModelExecutorImpl::OnModelExecuted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(form_data),
                         std::move(callback));
  optimization_guide::ExecuteModelWithLogging(
      &model_executor_.get(),
      optimization_guide::ModelBasedCapabilityKey::kFormsClassifications,
      request, kExecutionTimeout.Get(), std::move(wrapper_callback));
}

void AutofillAiModelExecutorImpl::OnModelExecuted(
    autofill::FormData form_data,
    PredictionsReceivedCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::proto::FormsClassificationsLoggingData>
        logging_data) {
  CHECK(logging_data);
  auto log_entry = std::make_unique<optimization_guide::ModelQualityLogEntry>(
      logs_uploader_);
  const std::optional<std::string> execution_id =
      logging_data->model_execution_info().execution_id();
  if (!execution_result.response.has_value()) {
    std::move(callback).Run(base::unexpected(false), execution_id);
    return;
  }

  SetLatestResponseForDebugging(
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::AutofillAiTypeResponse>(
          execution_result.response.value()));

  if (!GetLatestResponse()) {
    std::move(callback).Run(base::unexpected(false), execution_id);
    return;
  }

  std::move(callback).Run(ExtractPredictions(form_data, GetLatestResponse()),
                          execution_id);
}

// static
AutofillAiModelExecutor::PredictionsByGlobalId
AutofillAiModelExecutorImpl::ExtractPredictions(
    const autofill::FormData& form_data,
    const std::optional<optimization_guide::proto::AutofillAiTypeResponse>&
        model_response) {
  if (!model_response) {
    return {};
  }

  // TODO(crbug.com/389631477): Implement, but keep it simple - maybe just
  // pass on the proto.
  return {};
}

void AutofillAiModelExecutorImpl::SetLatestRequestForDebugging(
    optimization_guide::proto::AutofillAiTypeRequest request) {
  // Reset `latest_response_` to ensure it always matches `latest_request_`, if
  // it exists.
  latest_response_.reset();
  latest_request_ = std::move(request);
}

void AutofillAiModelExecutorImpl::SetLatestResponseForDebugging(
    std::optional<optimization_guide::proto::AutofillAiTypeResponse> response) {
  latest_response_ = std::move(response);
}

const std::optional<optimization_guide::proto::AutofillAiTypeRequest>&
AutofillAiModelExecutorImpl::GetLatestRequest() const {
  return latest_request_;
}

const std::optional<optimization_guide::proto::AutofillAiTypeResponse>&
AutofillAiModelExecutorImpl::GetLatestResponse() const {
  return latest_response_;
}

}  // namespace autofill_ai
