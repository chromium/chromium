// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_form_test_utils.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"

namespace autofill {

namespace test {

testing::Message DescribeFormData(const FormData& form_data) {
  testing::Message result;
  result << "Form contains " << form_data.fields.size() << " fields:\n";
  for (const FormFieldData& field : form_data.fields) {
    result << "type=" << FormControlTypeToString(field.form_control_type())
           << ", name=" << field.name() << ", label=" << field.label << "\n";
  }
  return result;
}

FormFieldData CreateFieldByRole(FieldType role) {
  FormFieldData field;
  switch (role) {
    case FieldType::USERNAME:
      field.label = u"Username";
      field.set_name(u"username");
      break;
    case FieldType::NAME_FULL:
      field.label = u"Full name";
      field.set_name(u"fullname");
      break;
    case FieldType::NAME_FIRST:
      field.label = u"First Name";
      field.set_name(u"firstName");
      break;
    case FieldType::NAME_LAST:
      field.label = u"Last Name";
      field.set_name(u"lastName");
      break;
    case FieldType::EMAIL_ADDRESS:
      field.label = u"E-mail address";
      field.set_name(u"email");
      break;
    case FieldType::ADDRESS_HOME_LINE1:
      field.label = u"Address";
      field.set_name(u"home_line_one");
      break;
    case FieldType::ADDRESS_HOME_CITY:
      field.label = u"City";
      field.set_name(u"city");
      break;
    case FieldType::ADDRESS_HOME_STATE:
      field.label = u"State";
      field.set_name(u"state");
      break;
    case FieldType::ADDRESS_HOME_COUNTRY:
      field.label = u"Country";
      field.set_name(u"country");
      break;
    case FieldType::ADDRESS_HOME_ZIP:
      field.label = u"Zip Code";
      field.set_name(u"zipCode");
      break;
    case FieldType::PHONE_HOME_NUMBER:
      field.label = u"Phone";
      field.set_name(u"phone");
      break;
    case FieldType::COMPANY_NAME:
      field.label = u"Company";
      field.set_name(u"company");
      break;
    case FieldType::CREDIT_CARD_NUMBER:
      field.label = u"Card Number";
      field.set_name(u"cardNumber");
      break;
    case FieldType::EMPTY_TYPE:
    default:
      break;
  }
  return field;
}

FormFieldData GetFormFieldData(const FieldDescription& fd) {
  FormFieldData ff = CreateFieldByRole(fd.role);
  ff.set_form_control_type(fd.form_control_type);
  if (ff.form_control_type() == FormControlType::kSelectOne &&
      !fd.select_options.empty()) {
    ff.options = fd.select_options;
  }

  ff.set_renderer_id(fd.renderer_id.value_or(MakeFieldRendererId()));
  ff.host_form_id = MakeFormRendererId();
  ff.is_focusable = fd.is_focusable;
  ff.is_visible = fd.is_visible;
  if (!fd.autocomplete_attribute.empty()) {
    ff.autocomplete_attribute = fd.autocomplete_attribute;
    ff.parsed_autocomplete =
        ParseAutocompleteAttribute(fd.autocomplete_attribute);
  }
  if (fd.host_frame) {
    ff.host_frame = *fd.host_frame;
  }
  if (fd.host_form_signature) {
    ff.host_form_signature = *fd.host_form_signature;
  }
  if (fd.label) {
    ff.label = *fd.label;
  }
  if (fd.name) {
    ff.set_name(*fd.name);
  }
  if (fd.value) {
    ff.set_value(*fd.value);
  }
  if (fd.placeholder) {
    ff.placeholder = *fd.placeholder;
  }
  if (fd.max_length) {
    ff.max_length = *fd.max_length;
  }
  if (fd.origin) {
    ff.origin = *fd.origin;
  }
  ff.is_autofilled = fd.is_autofilled.value_or(false);
  ff.should_autocomplete = fd.should_autocomplete;
  ff.properties_mask = fd.properties_mask;
  ff.check_status = fd.check_status;
  return ff;
}

FormData GetFormData(const FormDescription& d) {
  FormData f;
  f.url = GURL(d.url);
  f.action = GURL(d.action);
  f.name = d.name;
  f.host_frame = d.host_frame.value_or(MakeLocalFrameToken());
  f.renderer_id = d.renderer_id.value_or(MakeFormRendererId());
  if (d.main_frame_origin)
    f.main_frame_origin = *d.main_frame_origin;
  f.fields.reserve(d.fields.size());
  for (const FieldDescription& dd : d.fields) {
    FormFieldData ff = GetFormFieldData(dd);
    ff.host_frame = dd.host_frame.value_or(f.host_frame);
    ff.origin = dd.origin.value_or(f.main_frame_origin);
    ff.host_form_id = f.renderer_id;
    f.fields.push_back(ff);
  }
  return f;
}

std::vector<FieldType> GetHeuristicTypes(
    const FormDescription& form_description) {
  std::vector<FieldType> heuristic_types;
  heuristic_types.reserve(form_description.fields.size());

  for (const auto& field : form_description.fields) {
    heuristic_types.emplace_back(field.heuristic_type.value_or(field.role));
  }

  return heuristic_types;
}

std::vector<FieldType> GetServerTypes(const FormDescription& form_description) {
  std::vector<FieldType> server_types;
  server_types.reserve(form_description.fields.size());

  for (const auto& field : form_description.fields) {
    server_types.emplace_back(field.server_type.value_or(field.role));
  }

  return server_types;
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
      form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                              nullptr);

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

    if (test_case.form_flags.is_complete_credit_card_form.has_value()) {
      EXPECT_EQ(form_structure->IsCompleteCreditCardForm(),
                *test_case.form_flags.is_complete_credit_card_form);
    }
    if (test_case.form_flags.field_count) {
      ASSERT_EQ(*test_case.form_flags.field_count,
                static_cast<int>(form_structure->field_count()));
    }
    if (test_case.form_flags.autofill_count) {
      ASSERT_EQ(*test_case.form_flags.autofill_count,
                static_cast<int>(form_structure->autofill_count()));
    }
    if (test_case.form_flags.section_count) {
      std::set<Section> section_names;
      for (const auto& field : *form_structure)
        section_names.insert(field->section);
      EXPECT_EQ(*test_case.form_flags.section_count,
                static_cast<int>(section_names.size()));
    }

    for (size_t i = 0;
         i < test_case.expected_field_types.expected_html_type.size(); i++) {
      EXPECT_EQ(test_case.expected_field_types.expected_html_type[i],
                form_structure->field(i)->html_type());
    }
    for (size_t i = 0;
         i < test_case.expected_field_types.expected_heuristic_type.size();
         i++) {
      EXPECT_EQ(test_case.expected_field_types.expected_heuristic_type[i],
                form_structure->field(i)->heuristic_type());
    }
    for (size_t i = 0;
         i < test_case.expected_field_types.expected_overall_type.size(); i++) {
      EXPECT_EQ(test_case.expected_field_types.expected_overall_type[i],
                form_structure->field(i)->Type().GetStorableType());
    }
  }
}

}  // namespace test

}  // namespace autofill
