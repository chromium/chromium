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
    const FormData& form_data,
    const base::flat_map<FieldGlobalId, bool>& field_eligibility_map,
    const base::flat_map<FieldGlobalId, bool>& field_value_sensitivity_map) {
  optimization_guide::proto::FormData form_data_proto;
  form_data_proto.set_form_name(base::UTF16ToUTF8(form_data.name()));
  for (const auto& field : form_data.fields()) {
    auto* field_proto = form_data_proto.add_fields();

    // Unconditionally assign html meta data to the field.
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

    // Utility function to map the eligibility and value sensitivity to the form
    // data.
    auto map_is_true_for_key =
        [](const base::flat_map<FieldGlobalId, bool>& map, FieldGlobalId key) {
          auto it = map.find(key);
          return it != map.end() ? it->second : false;
        };

    field_proto->set_is_eligible(
        map_is_true_for_key(field_eligibility_map, field.global_id()));

    // Only forward the value if it was not identified as potentially sensitive.
    if (!map_is_true_for_key(field_value_sensitivity_map, field.global_id())) {
      field_proto->set_field_value(base::UTF16ToUTF8(field.value()));
    }
  }
  return form_data_proto;
}

optimization_guide::proto::FormData ToFormDataProto(
    const FormStructure& form_structure) {
  auto field_eligibility_map = base::MakeFlatMap<FieldGlobalId, bool>(
      form_structure.fields(), {}, [](const auto& field) {
        return std::make_pair(
            field->global_id(),
            field->field_is_eligible_for_prediction_improvements().value_or(
                false));
      });

  auto field_value_sensitivity_map =
      base::MakeFlatMap<FieldGlobalId, bool>(
          form_structure.fields(), {}, [](const auto& field) {
            return std::make_pair(
                field->global_id(),
                field->value_identified_as_potentially_sensitive());
          });

  FormData form_data = form_structure.ToFormData();
  return ToFormDataProto(form_data, field_eligibility_map,
                         field_value_sensitivity_map);
}

}  // namespace autofill
