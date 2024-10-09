// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine_impl.h"

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/user_annotations/user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"

namespace autofill_prediction_improvements {

AutofillPredictionImprovementsFillingEngineImpl::
    AutofillPredictionImprovementsFillingEngineImpl(
        optimization_guide::OptimizationGuideModelExecutor* model_executor,
        user_annotations::UserAnnotationsService* user_annotations_service)
    : model_executor_(model_executor),
      user_annotations_service_(user_annotations_service) {
  CHECK(model_executor_);
  CHECK(user_annotations_service_);
}
AutofillPredictionImprovementsFillingEngineImpl::
    ~AutofillPredictionImprovementsFillingEngineImpl() = default;

void AutofillPredictionImprovementsFillingEngineImpl::GetPredictions(
    autofill::FormData form_data,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    PredictionsReceivedCallback callback) {
  user_annotations_service_->RetrieveAllEntries(
      base::BindOnce(&AutofillPredictionImprovementsFillingEngineImpl::
                         OnUserAnnotationsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(form_data),
                     std::move(ax_tree_update), std::move(callback)));
}

void AutofillPredictionImprovementsFillingEngineImpl::
    OnUserAnnotationsRetrieved(
        autofill::FormData form_data,
        optimization_guide::proto::AXTreeUpdate ax_tree_update,
        PredictionsReceivedCallback callback,
        user_annotations::UserAnnotationsEntries user_annotations) {
  // At this point there should be user annotations. Return an error if there
  // aren't.
  // TODO(crbug.com/361414075): Check that `user_annotations` aren't empty in
  // `AutofillPredictionImprovementsDelegate::ShouldProvidePredictionImprovements()`.
  if (user_annotations.empty()) {
    std::move(callback).Run(base::unexpected(false), std::nullopt);
    return;
  }

  // Construct request.
  optimization_guide::proto::FormsPredictionsRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  page_context->set_url(form_data.url().spec());
  page_context->set_title(ax_tree_update.tree_data().title());
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update);
  *request.mutable_form_data() =
      ToFormDataProto(autofill::FormStructure(form_data));
  *request.mutable_entries() = {
      std::make_move_iterator(user_annotations.begin()),
      std::make_move_iterator(user_annotations.end())};

  model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, request,
      base::BindOnce(
          &AutofillPredictionImprovementsFillingEngineImpl::OnModelExecuted,
          weak_ptr_factory_.GetWeakPtr(), std::move(form_data),
          std::move(callback)));
}

void AutofillPredictionImprovementsFillingEngineImpl::OnModelExecuted(
    autofill::FormData form_data,
    PredictionsReceivedCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  const std::optional<std::string> execution_id =
      log_entry ? log_entry->model_execution_id()
                : std::optional<std::string>();
  if (!execution_result.has_value()) {
    std::move(callback).Run(base::unexpected(false), execution_id);
    return;
  }

  std::optional<optimization_guide::proto::FormsPredictionsResponse>
      maybe_response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::FormsPredictionsResponse>(
          execution_result.value());
  if (!maybe_response) {
    std::move(callback).Run(base::unexpected(false), execution_id);
    return;
  }

  std::move(callback).Run(
      ExtractPredictions(form_data, maybe_response->form_data()), execution_id);
}

// static
AutofillPredictionImprovementsFillingEngine::PredictionsByGlobalId
AutofillPredictionImprovementsFillingEngineImpl::ExtractPredictions(
    const autofill::FormData& form_data,
    const optimization_guide::proto::FilledFormData& form_data_proto) {
  std::vector<std::pair<autofill::FieldGlobalId, Prediction>> predictions;
  const std::vector<autofill::FormFieldData>& fields = form_data.fields();
  for (const optimization_guide::proto::FilledFormFieldData&
           filled_form_field_proto : form_data_proto.filled_form_field_data()) {
    // Only the first predicted value is used at the moment.
    if (filled_form_field_proto.predicted_values_size() == 0 ||
        filled_form_field_proto.predicted_values()[0].value().empty()) {
      continue;
    }

    // TODO(crbug.com/357098401): Change it to look by renderer ID which is
    // unique rather than label.
    if (auto it = base::ranges::find(
            fields,
            base::UTF8ToUTF16(
                filled_form_field_proto.field_data().field_label()),
            &autofill::FormFieldData::label);
        it != fields.end()) {
      const autofill::FormFieldData& field = *it;
      const std::u16string predicted_value = base::UTF8ToUTF16(
          filled_form_field_proto.predicted_values()[0].value());
      std::optional<std::u16string> select_option_text = std::nullopt;

      if (field.IsSelectElement()) {
        // Reject the prediction if it equals the currently selected option.
        if (field.selected_option().has_value() &&
            field.selected_option()->value == predicted_value) {
          continue;
        }

        // Ensure that the predicted value actually is one of the select
        // options.
        auto predicted_select_option_it = base::ranges::find(
            field.options(), predicted_value, &autofill::SelectOption::value);
        if (predicted_select_option_it == field.options().end()) {
          continue;
        }

        // Sets `Prediction::select_option_text` below which will then be shown
        // in the suggestion as the main text.
        select_option_text = predicted_select_option_it->text;
      }
      // Skip predictions for non-empty text fields.
      else if (field.IsTextInputElement() && !field.value().empty()) {
        continue;
      }

      const std::u16string label =
          filled_form_field_proto.normalized_label().empty()
              ? (field.label().empty() ? field.placeholder() : field.label())
              : base::UTF8ToUTF16(filled_form_field_proto.normalized_label());

      predictions.emplace_back(
          field.global_id(), Prediction{std::move(predicted_value),
                                        std::move(label), select_option_text});
    }
  }
  return predictions;
}

}  // namespace autofill_prediction_improvements
