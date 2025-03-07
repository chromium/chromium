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
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
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
    AutofillAiModelCache* model_cache,
    optimization_guide::OptimizationGuideModelExecutor* model_executor,
    optimization_guide::ModelQualityLogsUploaderService* logs_uploader)
    : model_cache_(CHECK_DEREF(model_cache)),
      model_executor_(CHECK_DEREF(model_executor)) {
  if (logs_uploader) {
    logs_uploader_ = logs_uploader->GetWeakPtr();
  }
}

AutofillAiModelExecutorImpl::~AutofillAiModelExecutorImpl() = default;

void AutofillAiModelExecutorImpl::GetPredictions(
    FormData form_data,
    PredictionCallback callback) {
  // If there is already an ongoing request for the same form signature, then
  // do not start a new one.
  if (!ongoing_queries_.insert(CalculateFormSignature(form_data)).second) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Construct request.
  AutofillAiTypeRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  if (features::kAutofillAiServerModelSendPageUrl.Get()) {
    page_context->set_url(form_data.url().spec());
  } else {
    page_context->set_url(form_data.main_frame_origin().Serialize());
  }

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
  const FormSignature form_signature = CalculateFormSignature(form_data);
  ongoing_queries_.erase(form_signature);

  auto log_entry = std::make_unique<optimization_guide::ModelQualityLogEntry>(
      logs_uploader_);
  if (!execution_result.response.has_value()) {
    // Save the response in the model to avoid that the client keeps querying
    // it even in the case in which responses are throttled.
    model_cache_->Update(form_signature, {}, {});
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<AutofillAiTypeResponse> response =
      optimization_guide::ParsedAnyMetadata<AutofillAiTypeResponse>(
          execution_result.response.value());

  if (!response) {
    // Save the response in the model to avoid that the client keeps querying
    // it even in the case in which responses are throttled.
    model_cache_->Update(form_signature, {}, {});
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Write response to model cache.
  // TODO(crbug.com/389631477): Parse response properly once the server starts
  // returning field information.
  model_cache_->Update(form_signature, {}, {});

  std::move(callback).Run(std::move(response).value());
}

}  // namespace autofill
