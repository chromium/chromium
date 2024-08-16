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
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/user_annotations/user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"

namespace autofill_prediction_improvements {

namespace {

// Converts `form_control_type` to its corresponding proto enum.
optimization_guide::proto::FormControlType ToFormControlTypeProto(
    autofill::FormControlType form_control_type) {
  switch (form_control_type) {
    case autofill::FormControlType::kContentEditable:
      return optimization_guide::proto::FORM_CONTROL_TYPE_CONTENT_EDITABLE;
    case autofill::FormControlType::kInputCheckbox:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_CHECKBOX;
    case autofill::FormControlType::kInputEmail:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_EMAIL;
    case autofill::FormControlType::kInputMonth:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_MONTH;
    case autofill::FormControlType::kInputNumber:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_NUMBER;
    case autofill::FormControlType::kInputPassword:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_PASSWORD;
    case autofill::FormControlType::kInputRadio:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_RADIO;
    case autofill::FormControlType::kInputSearch:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_SEARCH;
    case autofill::FormControlType::kInputTelephone:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TELEPHONE;
    case autofill::FormControlType::kInputText:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TEXT;
    case autofill::FormControlType::kInputUrl:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_URL;
    case autofill::FormControlType::kSelectOne:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_ONE;
    case autofill::FormControlType::kSelectMultiple:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_MULTIPLE;
    case autofill::FormControlType::kSelectList:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_LIST;
    case autofill::FormControlType::kTextArea:
      return optimization_guide::proto::FORM_CONTROL_TYPE_TEXT_AREA;
  }
  return optimization_guide::proto::FORM_CONTROL_TYPE_UNSPECIFIED;
}

// Converts `form_data` to its corresponding form data proto.
optimization_guide::proto::FormData ToFormDataProto(
    const autofill::FormData& form_data) {
  optimization_guide::proto::FormData form_data_proto;
  form_data_proto.set_form_name(base::UTF16ToUTF8(form_data.name()));
  for (const auto& field : form_data.fields()) {
    auto* field_proto = form_data_proto.add_fields();
    field_proto->set_field_name(base::UTF16ToUTF8(field.name()));
    field_proto->set_field_label(base::UTF16ToUTF8(field.label()));
    field_proto->set_is_visible(field.is_visible());
    field_proto->set_is_focusable(field.is_focusable());
    field_proto->set_placeholder(base::UTF16ToUTF8(field.placeholder()));
    field_proto->set_form_control_type(
        ToFormControlTypeProto(field.form_control_type()));
    for (const auto& option : field.options()) {
      auto* select_option_proto = field_proto->add_select_options();
      select_option_proto->set_value(base::UTF16ToUTF8(option.value));
      select_option_proto->set_text(base::UTF16ToUTF8(option.text));
    }
    field_proto->set_form_control_ax_node_id(field.form_control_ax_id());
  }
  return form_data_proto;
}

}  // namespace

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
        std::vector<optimization_guide::proto::UserAnnotationsEntry>
            user_annotations) {
  // If no user annotations, just return the callback with the original form.
  if (user_annotations.empty()) {
    std::move(callback).Run(std::move(form_data));
    return;
  }

  // Construct request.
  optimization_guide::proto::FormsPredictionsRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update);
  page_context->set_url(form_data.url().spec());
  page_context->set_title(ax_tree_update.tree_data().title());
  *request.mutable_form_data() = ToFormDataProto(form_data);
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
  if (!execution_result.has_value()) {
    std::move(callback).Run(base::unexpected(false));
    return;
  }

  std::optional<optimization_guide::proto::FormsPredictionsResponse>
      maybe_response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::FormsPredictionsResponse>(
          execution_result.value());
  if (!maybe_response) {
    std::move(callback).Run(base::unexpected(false));
    return;
  }

  FillFormDataWithResponse(form_data, maybe_response->form_data());
  std::move(callback).Run(std::move(form_data));
}

// static
void AutofillPredictionImprovementsFillingEngineImpl::FillFormDataWithResponse(
    autofill::FormData& form_data,
    const optimization_guide::proto::FilledFormData& form_data_proto) {
  std::vector<autofill::FormFieldData>& fields =
      form_data.mutable_fields(/*pass_key=*/{});
  for (const auto& filled_form_field_proto :
       form_data_proto.filled_form_field_data()) {
    // TODO: b/357098401 - Change it to look by renderer ID which is unique
    // rather than label.
    if (auto it = base::ranges::find(
            fields,
            base::UTF8ToUTF16(
                filled_form_field_proto.field_data().field_label()),
            &autofill::FormFieldData::label);
        it != fields.end()) {
      it->set_value(base::UTF8ToUTF16(
          filled_form_field_proto.field_data().field_value()));
    }
  }
}

}  // namespace autofill_prediction_improvements
