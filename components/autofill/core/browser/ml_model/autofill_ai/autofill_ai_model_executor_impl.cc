// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"

#include <optional>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace autofill {

using optimization_guide::proto::AutofillAiTypeRequest;
using optimization_guide::proto::AutofillAiTypeResponse;

AutofillAiModelExecutorImpl::AutofillAiModelExecutorImpl(
    optimization_guide::OptimizationGuideModelExecutor* model_executor,
    optimization_guide::ModelQualityLogsUploaderService* logs_uploader)
    : model_executor_(CHECK_DEREF(model_executor)) {
  if (logs_uploader) {
    logs_uploader_ = logs_uploader->GetWeakPtr();
  }
}

AutofillAiModelExecutorImpl::~AutofillAiModelExecutorImpl() = default;

void AutofillAiModelExecutorImpl::GetPredictions(
    FormData form_data,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    PredictionCallback callback) {
  // Construct request.
  AutofillAiTypeRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  if (features::kAutofillAiServerModelSendPageTitleAndUrl.Get()) {
    page_context->set_url(form_data.url().spec());
    page_context->set_title(ax_tree_update.tree_data().title());
  } else {
    page_context->set_url(form_data.main_frame_origin().Serialize());
  }
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update);

  *request.mutable_form_data() = ToFormDataProto(form_data);

  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::FormsClassificationsLoggingData>
      wrapper_callback =
          base::BindOnce(&AutofillAiModelExecutorImpl::OnModelExecuted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(form_data),
                         std::move(callback));
  optimization_guide::ExecuteModelWithLogging(
      &model_executor_.get(),
      optimization_guide::ModelBasedCapabilityKey::kFormsClassifications,
      request, features::kAutofillAiServerModelExecutionTimeout.Get(),
      std::move(wrapper_callback));
}

void AutofillAiModelExecutorImpl::OnModelExecuted(
    FormData form_data,
    PredictionCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::proto::FormsClassificationsLoggingData>
        logging_data) {
  auto log_entry = std::make_unique<optimization_guide::ModelQualityLogEntry>(
      logs_uploader_);
  if (!execution_result.response.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<AutofillAiTypeResponse> response =
      optimization_guide::ParsedAnyMetadata<AutofillAiTypeResponse>(
          execution_result.response.value());

  if (!response) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(std::move(response).value());
}

}  // namespace autofill
