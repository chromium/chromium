// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor_impl.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
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
    optimization_guide::ModelQualityLogsUploaderService* mqls_uploader)
    : model_cache_(CHECK_DEREF(model_cache)),
      model_executor_(CHECK_DEREF(model_executor)),
      mqls_uploader_(mqls_uploader) {}

AutofillAiModelExecutorImpl::~AutofillAiModelExecutorImpl() = default;

void AutofillAiModelExecutorImpl::GetPredictions(
    FormData form_data,
    base::OnceCallback<void(const FormGlobalId&)> on_model_executed,
    std::optional<optimization_guide::proto::AnnotatedPageContent>
        annotated_page_content) {
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

  if (annotated_page_content) {
    *request.mutable_annotated_page_content() =
        *std::move(annotated_page_content);
  }

  FormGlobalId form_id = form_data.global_id();
  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::FormsClassificationsLoggingData>
      wrapper_callback =
          base::BindOnce(&AutofillAiModelExecutorImpl::OnModelExecuted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(form_data))
              .Then(base::BindOnce(std::move(on_model_executed), form_id));
  optimization_guide::ExecuteModelWithLogging(
      &model_executor_.get(),
      optimization_guide::ModelBasedCapabilityKey::kFormsClassifications,
      request, features::kAutofillAiServerModelExecutionTimeout.Get(),
      std::move(wrapper_callback));
}

base::WeakPtr<AutofillAiModelExecutor>
AutofillAiModelExecutorImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillAiModelExecutorImpl::OnModelExecuted(
    FormData form_data,
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::proto::FormsClassificationsLoggingData>
        logging_data) {
  const FormSignature form_signature = CalculateFormSignature(form_data);
  ongoing_queries_.erase(form_signature);

  if (!execution_result.response.has_value()) {
    // Save the response in the model to avoid that the client keeps querying
    // it even in the case in which responses are throttled.
    model_cache_->Update(form_signature, {}, {});
    base::UmaHistogramEnumeration(
        kUmaAutofillAiModelExecutionStatus,
        AutofillAiModelExecutionStatus::kErrorServerError);
    return;
  }

  std::optional<AutofillAiTypeResponse> response =
      optimization_guide::ParsedAnyMetadata<AutofillAiTypeResponse>(
          execution_result.response.value());

  if (!response) {
    // Save the response in the model to avoid that the client keeps querying
    // it even in the case in which responses are throttled.
    model_cache_->Update(form_signature, {}, {});
    base::UmaHistogramEnumeration(
        kUmaAutofillAiModelExecutionStatus,
        AutofillAiModelExecutionStatus::kErrorWrongResponseType);
    return;
  }

  LogModelPredictions(std::move(logging_data));
  const size_t response_size = response->field_responses_size();
  if (response_size == 0) {
    model_cache_->Update(form_signature, {}, {});
    base::UmaHistogramEnumeration(
        kUmaAutofillAiModelExecutionStatus,
        AutofillAiModelExecutionStatus::kSuccessEmptyResult);
    return;
  }

  // Perform sanity checks: Every field index must occur only once and they
  // must all lie in the interval [0, form_data_fields().size() - 1].
  using optimization_guide::proto::FieldTypeResponse;
  std::vector<int> indices = base::ToVector(response->field_responses(),
                                            &FieldTypeResponse::field_index);
  std::ranges::sort(indices);
  auto repeated = std::ranges::unique(indices);
  indices.erase(repeated.begin(), repeated.end());
  if (indices.size() != response_size || indices.front() < 0 ||
      indices.back() >= static_cast<int>(form_data.fields().size())) {
    model_cache_->Update(form_signature, {}, {});
    base::UmaHistogramEnumeration(
        kUmaAutofillAiModelExecutionStatus,
        AutofillAiModelExecutionStatus::kErrorInvalidFieldIndex);
    return;
  }

  // First compute all field identifiers because we cannot assume that
  // `field_index` in the response is monotonically increasing.
  std::map<FieldSignature, size_t> field_ranks_in_signature_group;
  std::vector<AutofillAiModelCache::FieldIdentifier> all_field_identifiers;
  all_field_identifiers.reserve(form_data.fields().size());
  for (const FormFieldData& field : form_data.fields()) {
    const FieldSignature field_signature =
        CalculateFieldSignatureForField(field);
    all_field_identifiers.push_back(
        {.signature = field_signature,
         .rank_in_signature_group =
             field_ranks_in_signature_group[field_signature]++});
  }

  // Now select the identifiers corresponding to fields in the response.
  std::vector<AutofillAiModelCache::FieldIdentifier>
      relevant_field_identifiers = base::ToVector(
          response->field_responses(),
          [&all_field_identifiers](const FieldTypeResponse& field_response) {
            return all_field_identifiers[field_response.field_index()];
          });
  model_cache_->Update(form_signature, *std::move(response),
                       std::move(relevant_field_identifiers));
  base::UmaHistogramEnumeration(
      kUmaAutofillAiModelExecutionStatus,
      AutofillAiModelExecutionStatus::kSuccessNonEmptyResult);
}

void AutofillAiModelExecutorImpl::LogModelPredictions(
    std::unique_ptr<optimization_guide::proto::FormsClassificationsLoggingData>
        logging_data) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiUploadModelRequestAndResponse) ||
      !mqls_uploader_) {
    return;
  }
  // Note that the logging happens when `log_entry` goes out of scope.
  // Since the user was allowed to run the model, logging is ok.
  optimization_guide::ModelQualityLogEntry log_entry(
      mqls_uploader_->GetWeakPtr());
  optimization_guide::proto::FormsClassificationsLoggingData* data =
      log_entry.log_ai_data_request()->mutable_forms_classifications();
  // Only log the form and field signatures of the request, and the response.
  const optimization_guide::proto::FormData& request_form =
      logging_data->request().form_data();
  AutofillAiTypeRequest* stripped_request = data->mutable_request();
  optimization_guide::proto::FormData* stripped_form =
      stripped_request->mutable_form_data();
  stripped_form->set_form_signature(request_form.form_signature());
  for (const optimization_guide::proto::FormFieldData& field :
       request_form.fields()) {
    stripped_form->add_fields()->set_field_signature(field.field_signature());
  }
  *data->mutable_response() = std::move(logging_data->response());
}

}  // namespace autofill
