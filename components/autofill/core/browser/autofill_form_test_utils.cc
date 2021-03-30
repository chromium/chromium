// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_form_test_utils.h"

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"

using base::ASCIIToUTF16;

namespace autofill {

namespace test {

testing::Message DescribeFormData(const FormData& form_data) {
  testing::Message result;
  result << "Form contains " << form_data.fields.size() << " fields:\n";
  for (const FormFieldData& field : form_data.fields) {
    result << "type=" << field.form_control_type << ", name=" << field.name
           << ", label=" << field.label << "\n";
  }
  return result;
}

FormFieldData CreateFieldByRole(ServerFieldType role) {
  FormFieldData field;

  switch (role) {
    case ServerFieldType::USERNAME:
      field.label = u"Username";
      field.name = u"username";
      break;
    case ServerFieldType::NAME_FULL:
      field.label = u"Full name";
      field.name = u"fullname";
      break;
    case ServerFieldType::NAME_FIRST:
      field.label = u"First Name";
      field.name = u"firstName";
      break;
    case ServerFieldType::NAME_LAST:
      field.label = u"Last Name";
      field.name = u"lastName";
      break;
    case ServerFieldType::EMAIL_ADDRESS:
      field.label = u"E-mail address";
      field.name = u"email";
      break;
    case ServerFieldType::ADDRESS_HOME_LINE1:
      field.label = u"Address";
      field.name = u"home_line_one";
      break;
    case ServerFieldType::ADDRESS_HOME_CITY:
      field.label = u"City";
      field.name = u"city";
      break;
    case ServerFieldType::ADDRESS_HOME_STATE:
      field.label = u"State";
      field.name = u"state";
      break;
    case ServerFieldType::ADDRESS_HOME_COUNTRY:
      field.label = u"Country";
      field.name = u"country";
      break;
    case ServerFieldType::ADDRESS_HOME_ZIP:
      field.label = u"Zip Code";
      field.name = u"zipCode";
      break;
    case ServerFieldType::PHONE_HOME_NUMBER:
      field.label = u"Phone";
      field.name = u"phone";
      break;
    case ServerFieldType::COMPANY_NAME:
      field.label = u"Company";
      field.name = u"company";
      break;
    case ServerFieldType::CREDIT_CARD_NUMBER:
      field.label = u"Card Number";
      field.name = u"cardNumber";
      break;
    case ServerFieldType::EMPTY_TYPE:
    default:
      break;
  }

  return field;
}

FormData GetFormData(const TestFormAttributes& test_form_attributes) {
  FormData form_data;

  form_data.url = GURL(test_form_attributes.url);
  form_data.action = GURL(test_form_attributes.action);
  form_data.name = ASCIIToUTF16(test_form_attributes.name);
  static int field_count = 0;
  if (test_form_attributes.unique_renderer_id)
    form_data.unique_renderer_id = *test_form_attributes.unique_renderer_id;
  if (test_form_attributes.main_frame_origin)
    form_data.main_frame_origin = *test_form_attributes.main_frame_origin;
  for (const FieldDataDescription& field_description :
       test_form_attributes.fields) {
    FormFieldData field = CreateFieldByRole(field_description.role);
    field.form_control_type = field_description.form_control_type;
    field.is_focusable = field_description.is_focusable;
    if (field_description.autocomplete_attribute)
      field.autocomplete_attribute = field_description.autocomplete_attribute;
    if (ASCIIToUTF16(field_description.label) != ASCIIToUTF16(kLabelText))
      field.label = ASCIIToUTF16(field_description.label);
    if (ASCIIToUTF16(field_description.name) != ASCIIToUTF16(kNameText))
      field.name = ASCIIToUTF16(field_description.name);
    if (field_description.value)
      field.value = ASCIIToUTF16(*field_description.value);
    if (field_description.is_autofilled)
      field.is_autofilled = *field_description.is_autofilled;
    field.unique_renderer_id = FieldRendererId(field_count++);
    field.should_autocomplete = field_description.should_autocomplete;
    form_data.fields.push_back(field);
  }
  form_data.is_form_tag = test_form_attributes.is_form_tag;

  return form_data;
}

// static
void FormStructureTest::CheckFormStructureTestData(
    const std::vector<FormStructureTestCase>& test_cases) {
  for (const FormStructureTestCase& test_case : test_cases) {
    const FormData form = GetFormData(test_case.form_attributes);
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.form_attributes.description_for_logging);

    auto form_structure = std::make_unique<FormStructure>(form);

    if (test_case.form_flags.determine_heuristic_type)
      form_structure->DetermineHeuristicTypes(nullptr, nullptr);

    if (test_case.form_flags.is_autofillable)
      EXPECT_TRUE(form_structure->IsAutofillable());
    if (test_case.form_flags.should_be_parsed)
      EXPECT_TRUE(form_structure->ShouldBeParsed());
    if (test_case.form_flags.should_be_queried)
      EXPECT_TRUE(form_structure->ShouldBeQueried());
    if (test_case.form_flags.should_be_uploaded)
      EXPECT_TRUE(form_structure->ShouldBeUploaded());
    if (test_case.form_flags.has_author_specified_types)
      EXPECT_TRUE(form_structure->has_author_specified_types());
    if (test_case.form_flags.has_author_specified_upi_vpa_hint)
      EXPECT_TRUE(form_structure->has_author_specified_upi_vpa_hint());

    if (test_case.form_flags.is_complete_credit_card_form.first) {
      if (test_case.form_flags.is_complete_credit_card_form.second)
        EXPECT_TRUE(form_structure->IsCompleteCreditCardForm());
      else
        EXPECT_FALSE(form_structure->IsCompleteCreditCardForm());
    }

    if (test_case.form_flags.field_count)
      ASSERT_EQ(*test_case.form_flags.field_count,
                static_cast<int>(form_structure->field_count()));
    if (test_case.form_flags.autofill_count)
      ASSERT_EQ(*test_case.form_flags.autofill_count,
                static_cast<int>(form_structure->autofill_count()));
    if (test_case.form_flags.section_count) {
      std::set<std::string> section_names;
      for (size_t i = 0; i < 9; ++i) {
        section_names.insert(form_structure->field(i)->section);
      }
      EXPECT_EQ(*test_case.form_flags.section_count,
                static_cast<int>(section_names.size()));
    }

    if (!test_case.expected_field_types.expected_html_type.empty()) {
      for (size_t i = 0;
           i < test_case.expected_field_types.expected_html_type.size(); i++)
        EXPECT_EQ(test_case.expected_field_types.expected_html_type[i],
                  form_structure->field(i)->html_type());
    }
    if (!test_case.expected_field_types.expected_phone_part.empty()) {
      for (size_t i = 0;
           i < test_case.expected_field_types.expected_phone_part.size(); i++)
        EXPECT_EQ(test_case.expected_field_types.expected_phone_part[i],
                  form_structure->field(i)->phone_part());
    }
    if (!test_case.expected_field_types.expected_heuristic_type.empty()) {
      for (size_t i = 0;
           i < test_case.expected_field_types.expected_heuristic_type.size();
           i++)
        EXPECT_EQ(test_case.expected_field_types.expected_heuristic_type[i],
                  form_structure->field(i)->heuristic_type());
    }
    if (!test_case.expected_field_types.expected_overall_type.empty()) {
      for (size_t i = 0;
           i < test_case.expected_field_types.expected_overall_type.size(); i++)
        EXPECT_EQ(test_case.expected_field_types.expected_overall_type[i],
                  form_structure->field(i)->Type().GetStorableType());
    }
  }
}

}  // namespace test

}  // namespace autofill
