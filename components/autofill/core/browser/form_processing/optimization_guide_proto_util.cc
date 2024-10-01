// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_proto_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
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
    case FormControlType::kSelectMultiple:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_MULTIPLE;
    case FormControlType::kTextArea:
      return optimization_guide::proto::FORM_CONTROL_TYPE_TEXT_AREA;
  }
  return optimization_guide::proto::FORM_CONTROL_TYPE_UNSPECIFIED;
}

}  // namespace

optimization_guide::proto::FormData ToFormDataProto(
    const FormStructure& form_structure) {
  optimization_guide::proto::FormData form_data_proto;
  form_data_proto.set_form_name(base::UTF16ToUTF8(form_structure.form_name()));
  for (const auto& field : form_structure) {
    auto* field_proto = form_data_proto.add_fields();
    field_proto->set_field_name(base::UTF16ToUTF8(field->name()));
    field_proto->set_field_label(base::UTF16ToUTF8(field->label()));
    // Only forward the value if it was not identified as potentially sensitive.
    if (!field->value_identified_as_potentially_sensitive()) {
      field_proto->set_field_value(
          base::UTF16ToUTF8(field->value(ValueSemantics::kCurrent)));
    }
    field_proto->set_is_visible(field->is_visible());
    field_proto->set_is_focusable(field->is_focusable());
    field_proto->set_placeholder(base::UTF16ToUTF8(field->placeholder()));
    field_proto->set_form_control_type(
        ToFormControlTypeProto(field->form_control_type()));
    for (const auto& option : field->options()) {
      auto* select_option_proto = field_proto->add_select_options();
      select_option_proto->set_value(base::UTF16ToUTF8(option.value));
      select_option_proto->set_text(base::UTF16ToUTF8(option.text));
    }
    field_proto->set_form_control_ax_node_id(field->form_control_ax_id());
  }
  return form_data_proto;
}

}  // namespace autofill
