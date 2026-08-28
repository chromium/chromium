// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/test_utils/autofill_form_test_util.h"

namespace autofill::test {

testing::Matcher<FormFieldData> FormFieldDescriptionEq(
    const test::CommonFieldDescription& expected) {
  std::vector<testing::Matcher<FormFieldData>> matchers;
  const auto add_member_checker = [&matchers]<typename T>(
                                      const std::string& member_name,
                                      auto member_pointer,
                                      const std::optional<T>& expected_value) {
    if (expected_value) {
      matchers.push_back(
          testing::Property(member_name, member_pointer, *expected_value));
    }
  };

  // LINT.IfChange(FormFieldDescriptionEq)
  add_member_checker("host_frame", &FormFieldData::host_frame,
                     expected.host_frame);
  add_member_checker("host_form_signature", &FormFieldData::host_form_signature,
                     expected.host_form_signature);
  add_member_checker("renderer_id", &FormFieldData::renderer_id,
                     expected.renderer_id);
  add_member_checker("is_focusable", &FormFieldData::is_focusable,
                     expected.is_focusable);
  add_member_checker("is_visible", &FormFieldData::is_visible,
                     expected.is_visible);
  add_member_checker("label", &FormFieldData::label, expected.label);
  add_member_checker("name", &FormFieldData::name, expected.name);
  add_member_checker("name_attribute", &FormFieldData::name_attribute,
                     expected.name_attribute);
  add_member_checker("id_attribute", &FormFieldData::id_attribute,
                     expected.id_attribute);
  add_member_checker("nonce", &FormFieldData::nonce, expected.nonce);
  add_member_checker("value", &FormFieldData::value, expected.value);
  add_member_checker("placeholder", &FormFieldData::placeholder,
                     expected.placeholder);
  add_member_checker("placeholder_attribute",
                     &FormFieldData::placeholder_attribute,
                     expected.placeholder_attribute);
  add_member_checker("aria_label", &FormFieldData::aria_label,
                     expected.aria_label);
  add_member_checker("aria_description", &FormFieldData::aria_description,
                     expected.aria_description);
  add_member_checker("max_length", &FormFieldData::max_length,
                     expected.max_length);
  add_member_checker("autocomplete_attribute",
                     &FormFieldData::autocomplete_attribute,
                     expected.autocomplete_attribute);
  add_member_checker("parsed_autocomplete", &FormFieldData::parsed_autocomplete,
                     expected.parsed_autocomplete);
  add_member_checker("form_control_type", &FormFieldData::form_control_type,
                     expected.form_control_type);
  add_member_checker("should_autocomplete", &FormFieldData::should_autocomplete,
                     expected.should_autocomplete);
  add_member_checker("is_autofilled_according_to_renderer",
                     &FormFieldData::is_autofilled_according_to_renderer,
                     expected.is_autofilled_according_to_renderer);
  add_member_checker("origin", &FormFieldData::origin, expected.origin);
  add_member_checker("options", &FormFieldData::options,
                     expected.select_options);
  add_member_checker("datalist_options", &FormFieldData::datalist_options,
                     expected.datalist_options);
  add_member_checker("properties_mask", &FormFieldData::properties_mask,
                     expected.properties_mask);
  add_member_checker("form_control_ax_id", &FormFieldData::form_control_ax_id,
                     expected.form_control_ax_id);
  add_member_checker("label_source", &FormFieldData::label_source,
                     expected.label_source);
  add_member_checker("pattern", &FormFieldData::pattern, expected.pattern);
  add_member_checker("css_classes", &FormFieldData::css_classes,
                     expected.css_classes);
  add_member_checker("text_direction", &FormFieldData::text_direction,
                     expected.text_direction);
  // LINT.ThenChange(//components/autofill/core/common/test_utils/autofill_form_test_util.h:FieldDescriptionDataMembers)

  return testing::AllOfArray(matchers);
}

}  // namespace autofill::test
