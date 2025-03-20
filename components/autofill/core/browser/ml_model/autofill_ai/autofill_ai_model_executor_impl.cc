// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"

#include <optional>
#include <utility>
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

void AutofillAiModelExecutorImpl::GetPredictions(FormData form_data) {
  // If there is already an ongoing request for the same form signature, then
  // do not start a new one.
  if (!ongoing_queries_.insert(CalculateFormSignature(form_data)).second) {
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

  *request.mutable_form_data() =
      ToFormDataProto(form_data, FormDataProtoConversionReason::kModelRequest);

  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::FormsClassificationsLoggingData>
      wrapper_callback =
          base::BindOnce(&AutofillAiModelExecutorImpl::OnModelExecuted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(form_data));
  optimization_guide::ExecuteModelWithLogging(
      &model_executor_.get(),
      optimization_guide::ModelBasedCapabilityKey::kFormsClassifications,
      request, features::kAutofillAiServerModelExecutionTimeout.Get(),
      std::move(wrapper_callback));
}

void AutofillAiModelExecutorImpl::OnModelExecuted(
    FormData form_data,
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::proto::FormsClassificationsLoggingData>
        logging_data) {
  const FormSignature form_signature = CalculateFormSignature(form_data);
  ongoing_queries_.erase(form_signature);

  // TODO(crbug.com/389631477): Populate the log entry.
  auto log_entry = std::make_unique<optimization_guide::ModelQualityLogEntry>(
      logs_uploader_);

  if (!execution_result.response.has_value()) {
    // Save the response in the model to avoid that the client keeps querying
    // it even in the case in which responses are throttled.
    model_cache_->Update(form_signature, {}, {});
    return;
  }

  std::optional<AutofillAiTypeResponse> response =
      optimization_guide::ParsedAnyMetadata<AutofillAiTypeResponse>(
          execution_result.response.value());

  if (!response) {
    // Save the response in the model to avoid that the client keeps querying
    // it even in the case in which responses are throttled.
    model_cache_->Update(form_signature, {}, {});
    return;
  }

  // TODO(crbug.com/389631477): Validate parsing assumptions with server team.
  // For now, we assume that we should receive as many fields in the response as
  // we sent in the request. If that is not the case, we treat it as an invalid
  // response.
  if (static_cast<size_t>(response->field_responses_size()) !=
      form_data.fields().size()) {
    model_cache_->Update(form_signature, {}, {});
    return;
  }

  std::map<FieldSignature, size_t> field_ranks_in_signature_group;
  std::vector<AutofillAiModelCache::FieldIdentifier> field_identifiers;
  field_identifiers.reserve(form_data.fields().size());
  for (const FormFieldData& field : form_data.fields()) {
    const FieldSignature field_signature =
        CalculateFieldSignatureForField(field);
    field_identifiers.push_back(
        {.signature = field_signature,
         .rank_in_signature_group =
             field_ranks_in_signature_group[field_signature]++});
  }
  model_cache_->Update(form_signature, *std::move(response),
                       std::move(field_identifiers));
}

}  // namespace autofill
