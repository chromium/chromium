// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"

#include <concepts>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace autofill {

namespace {

// Converts `form_control_type` to its corresponding proto enum.
optimization_guide::proto::FormControlType ToFormControlTypeProto(
    FormControlType form_control_type) {
  switch (form_control_type) {
    case FormControlType::kContentEditable:
      return optimization_guide::proto::FORM_CONTROL_TYPE_CONTENT_EDITABLE;
    case FormControlType::kInputCheckbox:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_CHECKBOX;
    case FormControlType::kInputEmail:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_EMAIL;
    case FormControlType::kInputDate:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_DATE;
    case FormControlType::kInputMonth:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_MONTH;
    case FormControlType::kInputNumber:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_NUMBER;
    case FormControlType::kInputPassword:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_PASSWORD;
    case FormControlType::kInputRadio:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_RADIO;
    case FormControlType::kInputSearch:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_SEARCH;
    case FormControlType::kInputTelephone:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TELEPHONE;
    case FormControlType::kInputText:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TEXT;
    case FormControlType::kInputUrl:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_URL;
    case FormControlType::kSelectOne:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_ONE;
    case FormControlType::kTextArea:
      return optimization_guide::proto::FORM_CONTROL_TYPE_TEXT_AREA;
  }
  return optimization_guide::proto::FORM_CONTROL_TYPE_UNSPECIFIED;
}

template <typename T>
  requires(std::same_as<T, FormGlobalId> || std::same_as<T, FieldGlobalId>)
optimization_guide::proto::AutofillGlobalId ToGlobalIdProto(T global_id) {
  optimization_guide::proto::AutofillGlobalId proto;
  proto.set_frame_token(global_id.frame_token.ToString());
  proto.set_renderer_id(global_id.renderer_id.value());
  return proto;
}

void PopulateExtensionAPISpecificFields(
    const FormData& form_data,
    optimization_guide::proto::FormData& form_proto) {
  // Add additional form-level metadata.
  *form_proto.mutable_global_id() = ToGlobalIdProto(form_data.global_id());
  // Add additional field-level metadata.
  CHECK_EQ(form_data.fields().size(),
           static_cast<size_t>(form_proto.fields_size()));
  for (size_t i = 0; i < form_data.fields().size(); i++) {
    const FormFieldData& field = form_data.fields()[i];
    *form_proto.mutable_fields(i)->mutable_global_id() =
        ToGlobalIdProto(field.global_id());
  }
}

}  // namespace

optimization_guide::proto::FormData ToFormDataProto(
    const FormData& form_data,
    FormDataProtoConversionReason conversion_reason) {
  optimization_guide::proto::FormData form_data_proto;
  form_data_proto.set_form_signature(*CalculateFormSignature(form_data));
  form_data_proto.set_form_name(base::UTF16ToUTF8(form_data.name()));
  for (const FormFieldData& field : form_data.fields()) {
    optimization_guide::proto::FormFieldData* field_proto =
        form_data_proto.add_fields();
    field_proto->set_field_signature(*CalculateFieldSignatureForField(field));
    // Unconditionally assign html meta data to the field.
    field_proto->set_field_name(base::UTF16ToUTF8(field.name()));
    field_proto->set_field_label(base::UTF16ToUTF8(field.label()));
    field_proto->set_aria_label(base::UTF16ToUTF8(field.aria_label()));
    field_proto->set_aria_description(
        base::UTF16ToUTF8(field.aria_description()));
    field_proto->set_is_focusable(field.is_focusable());
    field_proto->set_placeholder(base::UTF16ToUTF8(field.placeholder()));
    field_proto->set_form_control_type(
        ToFormControlTypeProto(field.form_control_type()));
    for (const SelectOption& option : field.options()) {
      optimization_guide::proto::SelectOption* select_option_proto =
          field_proto->add_select_options();
      select_option_proto->set_value(base::UTF16ToUTF8(option.value));
      select_option_proto->set_text(base::UTF16ToUTF8(option.text));
    }
    field_proto->set_form_control_ax_node_id(field.form_control_ax_id());
  }

  switch (conversion_reason) {
    case FormDataProtoConversionReason::kModelRequest:
      break;
    case FormDataProtoConversionReason::kExtensionAPI:
      PopulateExtensionAPISpecificFields(form_data, form_data_proto);
      break;
  }
  return form_data_proto;
}

}  // namespace autofill
