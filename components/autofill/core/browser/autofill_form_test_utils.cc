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
  result << "Form contains " << form_data.fields().size() << " fields:\n";
  for (const FormFieldData& field : form_data.fields()) {
    result << "type=" << FormControlTypeToString(field.form_control_type())
           << ", name=" << field.name() << ", label=" << field.label() << "\n";
  }
  return result;
}

FormFieldData CreateFieldByRole(FieldType role) {
  FormFieldData field;
  switch (role) {
    case FieldType::USERNAME:
      field.set_label(u"Username");
      field.set_name(u"username");
      break;
    case FieldType::NAME_FULL:
      field.set_label(u"Full name");
      field.set_name(u"fullname");
      break;
    case FieldType::NAME_FIRST:
      field.set_label(u"First Name");
      field.set_name(u"firstName");
      break;
    case FieldType::NAME_LAST:
      field.set_label(u"Last Name");
      field.set_name(u"lastName");
      break;
    case FieldType::EMAIL_ADDRESS:
      field.set_label(u"E-mail address");
      field.set_name(u"email");
      break;
    case FieldType::ADDRESS_HOME_LINE1:
      field.set_label(u"Address");
      field.set_name(u"home_line_one");
      break;
    case FieldType::ADDRESS_HOME_CITY:
      field.set_label(u"City");
      field.set_name(u"city");
      break;
    case FieldType::ADDRESS_HOME_STATE:
      field.set_label(u"State");
      field.set_name(u"state");
      break;
    case FieldType::ADDRESS_HOME_COUNTRY:
      field.set_label(u"Country");
      field.set_name(u"country");
      break;
    case FieldType::ADDRESS_HOME_ZIP:
      field.set_label(u"Zip Code");
      field.set_name(u"zipCode");
      break;
    case FieldType::PHONE_HOME_NUMBER:
      field.set_label(u"Phone");
      field.set_name(u"phone");
      break;
    case FieldType::COMPANY_NAME:
      field.set_label(u"Company");
      field.set_name(u"company");
      break;
    case FieldType::CREDIT_CARD_NUMBER:
      field.set_label(u"Card Number");
      field.set_name(u"cardNumber");
      break;
    case FieldType::PASSWORD:
      field.set_label(u"Password");
      field.set_name(u"password");
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
    ff.set_options(fd.select_options);
  }

  ff.set_renderer_id(fd.renderer_id.value_or(MakeFieldRendererId()));
  ff.set_host_form_id(MakeFormRendererId());
  ff.set_is_focusable(fd.is_focusable);
  ff.set_is_visible(fd.is_visible);
  if (!fd.autocomplete_attribute.empty()) {
    ff.set_autocomplete_attribute(fd.autocomplete_attribute);
    ff.set_parsed_autocomplete(
        ParseAutocompleteAttribute(fd.autocomplete_attribute));
  }
  if (fd.host_frame) {
    ff.set_host_frame(*fd.host_frame);
  }
  if (fd.host_form_signature) {
    ff.set_host_form_signature(*fd.host_form_signature);
  }
  if (fd.label) {
    ff.set_label(*fd.label);
  }
  if (fd.name) {
    ff.set_name(*fd.name);
  }
  if (fd.name_attribute) {
    ff.set_name_attribute(*fd.name_attribute);
  }
  if (fd.id_attribute) {
    ff.set_id_attribute(*fd.id_attribute);
  }
  if (fd.value) {
    ff.set_value(*fd.value);
  }
  if (fd.placeholder) {
    ff.set_placeholder(*fd.placeholder);
  }
  if (fd.max_length) {
    ff.set_max_length(*fd.max_length);
  }
  if (fd.origin) {
    ff.set_origin(*fd.origin);
  }
  ff.set_is_autofilled(fd.is_autofilled.value_or(false));
  ff.set_should_autocomplete(fd.should_autocomplete);
  ff.set_properties_mask(fd.properties_mask);
  ff.set_check_status(fd.check_status);
  return ff;
}

FormData GetFormData(const FormDescription& d) {
  FormData f;
  f.set_url(GURL(d.url));
  f.set_action(GURL(d.action));
  f.set_name(d.name);
  f.set_host_frame(d.host_frame.value_or(MakeLocalFrameToken()));
  f.set_renderer_id(d.renderer_id.value_or(MakeFormRendererId()));
  if (d.main_frame_origin) {
    f.set_main_frame_origin(*d.main_frame_origin);
  }
  std::vector<FormFieldData> fs;
  fs.reserve(d.fields.size());
  for (const FieldDescription& dd : d.fields) {
    FormFieldData ff = GetFormFieldData(dd);
    ff.set_host_frame(dd.host_frame.value_or(f.host_frame()));
    ff.set_origin(dd.origin.value_or(f.main_frame_origin()));
    ff.set_host_form_id(f.renderer_id());
    fs.push_back(ff);
  }
  f.set_fields(std::move(fs));
  return f;
}

FormData GetFormData(const std::vector<FieldType>& field_types) {
  FormDescription form_description;
  form_description.fields.reserve(field_types.size());
  for (FieldType type : field_types) {
    form_description.fields.emplace_back(type);
  }
  return GetFormData(form_description);
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
        section_names.insert(field->section());
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
