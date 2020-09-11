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
      field.label = ASCIIToUTF16("Username");
      field.name = ASCIIToUTF16("username");
      break;
    case ServerFieldType::NAME_FULL:
      field.label = ASCIIToUTF16("Full name");
      field.name = ASCIIToUTF16("fullname");
      break;
    case ServerFieldType::NAME_FIRST:
      field.label = ASCIIToUTF16("First Name");
      field.name = ASCIIToUTF16("firstName");
      break;
    case ServerFieldType::NAME_LAST:
      field.label = ASCIIToUTF16("Last Name");
      field.name = ASCIIToUTF16("lastName");
      break;
    case ServerFieldType::EMAIL_ADDRESS:
      field.label = ASCIIToUTF16("E-mail address");
      field.name = ASCIIToUTF16("email");
      break;
    case ServerFieldType::ADDRESS_HOME_CITY:
      field.label = ASCIIToUTF16("City");
      field.name = ASCIIToUTF16("city");
      break;
    case ServerFieldType::ADDRESS_HOME_STATE:
      field.label = ASCIIToUTF16("State");
      field.name = ASCIIToUTF16("state");
      break;
    case ServerFieldType::ADDRESS_HOME_COUNTRY:
      field.label = ASCIIToUTF16("Country");
      field.name = ASCIIToUTF16("country");
      break;
    case ServerFieldType::ADDRESS_HOME_ZIP:
      field.label = ASCIIToUTF16("Zip Code");
      field.name = ASCIIToUTF16("zipCode");
      break;
    case ServerFieldType::PHONE_HOME_NUMBER:
      field.label = ASCIIToUTF16("Phone");
      field.name = ASCIIToUTF16("phone");
      break;
    case ServerFieldType::COMPANY_NAME:
      field.label = ASCIIToUTF16("Company");
      field.name = ASCIIToUTF16("company");
      break;
    case ServerFieldType::CREDIT_CARD_NUMBER:
      field.label = ASCIIToUTF16("Card Number");
      field.name = ASCIIToUTF16("cardNumber");
      break;
    case ServerFieldType::EMPTY_TYPE:
    default:
      break;
  }

  return field;
}

FormData GetFormData(const FormAttributes& form_attributes) {
  FormData form_data;

  form_data.url = GURL(form_attributes.form_url);
  for (const FieldDataDescription& field_description : form_attributes.fields) {
    FormFieldData field = CreateFieldByRole(field_description.role);
    field.form_control_type = field_description.form_control_type;
    field.is_focusable = field_description.is_focusable;
    if (field_description.autocomplete_attribute)
      field.autocomplete_attribute = field_description.autocomplete_attribute;
    if (ASCIIToUTF16(field_description.label) != ASCIIToUTF16(kLabelText))
      field.label = ASCIIToUTF16(field_description.label);
    if (ASCIIToUTF16(field_description.name) != ASCIIToUTF16(kNameText))
      field.name = ASCIIToUTF16(field_description.name);
    field.should_autocomplete = field_description.should_autocomplete;
    form_data.fields.push_back(field);
  }
  form_data.is_formless_checkout = form_attributes.is_formless_checkout;
  form_data.is_form_tag = form_attributes.is_form_tag;

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
      form_structure->DetermineHeuristicTypes();

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
