// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_proto_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace optimization_guide {

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

}  // namespace

optimization_guide::proto::FormData ToFormDataProto(
    const autofill::FormData& form_data) {
  optimization_guide::proto::FormData form_data_proto;
  form_data_proto.set_form_name(base::UTF16ToUTF8(form_data.name()));
  for (const auto& field : form_data.fields()) {
    auto* field_proto = form_data_proto.add_fields();
    field_proto->set_field_name(base::UTF16ToUTF8(field.name()));
    field_proto->set_field_label(base::UTF16ToUTF8(field.label()));
    field_proto->set_field_value(base::UTF16ToUTF8(field.value()));
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

}  // namespace optimization_guide
