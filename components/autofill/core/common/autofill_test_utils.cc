// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_test_utils.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill::test {

using base::ASCIIToUTF16;

AutofillTestEnvironment* AutofillTestEnvironment::current_instance_ = nullptr;

AutofillTestEnvironment& AutofillTestEnvironment::GetCurrent(
    const base::Location& location) {
  CHECK(current_instance_)
      << location.ToString() << " "
      << "tried to access the current AutofillTestEnvironment, but none "
         "exists. Add an autofill::test::Autofill(Browser|Unit)TestEnvironment "
         "member to test your test fixture.";
  return *current_instance_;
}

AutofillTestEnvironment::AutofillTestEnvironment(const Options& options) {
  CHECK(!current_instance_) << "An autofill::test::AutofillTestEnvironment has "
                               "already been registered.";
  current_instance_ = this;
  if (options.disable_server_communication) {
    scoped_feature_list_.InitAndDisableFeature(
        features::test::kAutofillServerCommunication);
  }
}

AutofillTestEnvironment::~AutofillTestEnvironment() {
  CHECK_EQ(current_instance_, this);
  current_instance_ = nullptr;
}

LocalFrameToken AutofillTestEnvironment::NextLocalFrameToken() {
  return LocalFrameToken(base::UnguessableToken::CreateForTesting(
      ++local_frame_token_counter_high_, ++local_frame_token_counter_low_));
}

FormRendererId AutofillTestEnvironment::NextFormRendererId() {
  return FormRendererId(++form_renderer_id_counter_);
}

FieldRendererId AutofillTestEnvironment::NextFieldRendererId() {
  return FieldRendererId(++field_renderer_id_counter_);
}

AutofillBrowserTestEnvironment::AutofillBrowserTestEnvironment(
    const Options& options)
    : AutofillTestEnvironment(options) {}

LocalFrameToken MakeLocalFrameToken(RandomizeFrame randomize) {
  if (*randomize) {
    return LocalFrameToken(
        AutofillTestEnvironment::GetCurrent().NextLocalFrameToken());
  } else {
    return LocalFrameToken(
        base::UnguessableToken::CreateForTesting(98765, 43210));
  }
}

FormData WithoutValues(FormData form) {
  for (FormFieldData& field : form.fields) {
    field.value.clear();
  }
  return form;
}

FormData AsAutofilled(FormData form, bool is_autofilled) {
  for (FormFieldData& field : form.fields) {
    field.is_autofilled = is_autofilled;
  }
  return form;
}

FormData WithoutUnserializedData(FormData form) {
  form.url = {};
  form.main_frame_origin = {};
  form.host_frame = {};
  for (FormFieldData& field : form.fields) {
    field = WithoutUnserializedData(std::move(field));
  }
  return form;
}

FormFieldData WithoutUnserializedData(FormFieldData field) {
  field.host_frame = {};
  return field;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  std::string_view type) {
  FormFieldData field;
  CreateTestFormField(label, name, value, type, &field);
  return field;
}

void CreateTestFormField(std::string_view label,
                         std::string_view name,
                         std::string_view value,
                         std::string_view type,
                         FormFieldData* field) {
  field->host_frame = MakeLocalFrameToken();
  field->unique_renderer_id = MakeFieldRendererId();
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = type;
  field->is_focusable = true;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  std::string_view type,
                                  std::string_view autocomplete) {
  FormFieldData field;
  CreateTestFormField(label, name, value, type, autocomplete, &field);
  return field;
}

void CreateTestFormField(std::string_view label,
                         std::string_view name,
                         std::string_view value,
                         std::string_view type,
                         std::string_view autocomplete,
                         FormFieldData* field) {
  CreateTestFormField(label, name, value, type, field);
  field->autocomplete_attribute = autocomplete;
  field->parsed_autocomplete = ParseAutocompleteAttribute(autocomplete);
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  std::string_view type,
                                  std::string_view autocomplete,
                                  uint64_t max_length) {
  FormFieldData field;
  CreateTestFormField(label, name, value, type, autocomplete, max_length,
                      &field);
  return field;
}

void CreateTestFormField(std::string_view label,
                         std::string_view name,
                         std::string_view value,
                         std::string_view type,
                         std::string_view autocomplete,
                         uint64_t max_length,
                         FormFieldData* field) {
  // First, set the `max_length`, as the `parsed_autocomplete` is set based on
  // this value.
  field->max_length = max_length;
  CreateTestFormField(label, name, value, type, autocomplete, field);
}

FormFieldData CreateTestSelectField(std::string_view label,
                                    std::string_view name,
                                    std::string_view value,
                                    const std::vector<const char*>& values,
                                    const std::vector<const char*>& contents) {
  return CreateTestSelectField(label, name, value, /*autocomplete=*/"", values,
                               contents);
}

FormFieldData CreateTestSelectField(std::string_view label,
                                    std::string_view name,
                                    std::string_view value,
                                    std::string_view autocomplete,
                                    const std::vector<const char*>& values,
                                    const std::vector<const char*>& contents) {
  return CreateTestSelectOrSelectMenuField(label, name, value, autocomplete,
                                           values, contents,
                                           /*field_type=*/"select-one");
}

FormFieldData CreateTestSelectField(const std::vector<const char*>& values) {
  return CreateTestSelectField(/*label=*/"", /*name=*/"", /*value=*/"",
                               /*autocomplete=*/"", values,
                               /*contents=*/values);
}

FormFieldData CreateTestSelectOrSelectMenuField(
    std::string_view label,
    std::string_view name,
    std::string_view value,
    std::string_view autocomplete,
    const std::vector<const char*>& values,
    const std::vector<const char*>& contents,
    std::string_view field_type) {
  CHECK(field_type == "select-one" || field_type == "selectmenu");
  FormFieldData field = CreateTestFormField(label, name, value, field_type);
  field.autocomplete_attribute = autocomplete;
  field.parsed_autocomplete = ParseAutocompleteAttribute(autocomplete);

  CHECK_EQ(values.size(), contents.size());
  field.options.reserve(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    field.options.push_back({
        .value = base::UTF8ToUTF16(values[i]),
        .content = base::UTF8ToUTF16(contents[i]),
    });
  }
  return field;
}

FormFieldData CreateTestDatalistField(std::string_view label,
                                      std::string_view name,
                                      std::string_view value,
                                      const std::vector<const char*>& values,
                                      const std::vector<const char*>& labels) {
  // Fill the base attributes.
  FormFieldData field = CreateTestFormField(label, name, value, "text");

  field.datalist_values.reserve(values.size());
  for (const auto* x : values) {
    field.datalist_values.emplace_back(base::UTF8ToUTF16(x));
  }
  field.datalist_labels.reserve(labels.size());
  for (const auto* x : values) {
    field.datalist_labels.emplace_back(base::UTF8ToUTF16(x));
  }

  return field;
}

FormData CreateTestPersonalInformationFormData() {
  FormData form;
  CreateTestPersonalInformationFormData(&form);
  return form;
}

void CreateTestPersonalInformationFormData(FormData* form) {
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm";
  form->url = GURL("https://myform.com/form.html");
  form->action = GURL("https://myform.com/submit.html");
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
}

FormData CreateTestCreditCardFormData(bool is_https,
                                      bool use_month_type,
                                      bool split_names) {
  FormData form;
  CreateTestCreditCardFormData(&form, is_https, use_month_type, split_names);
  return form;
}

void CreateTestCreditCardFormData(FormData* form,
                                  bool is_https,
                                  bool use_month_type,
                                  bool split_names) {
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm";
  if (is_https) {
    form->url = GURL("https://myform.com/form.html");
    form->action = GURL("https://myform.com/submit.html");
    form->main_frame_origin =
        url::Origin::Create(GURL("https://myform_root.com/form.html"));
  } else {
    form->url = GURL("http://myform.com/form.html");
    form->action = GURL("http://myform.com/submit.html");
    form->main_frame_origin =
        url::Origin::Create(GURL("http://myform_root.com/form.html"));
  }

  FormFieldData field;
  if (split_names) {
    test::CreateTestFormField("First Name on Card", "firstnameoncard", "",
                              "text", &field);
    field.autocomplete_attribute = "cc-given-name";
    form->fields.push_back(field);
    test::CreateTestFormField("Last Name on Card", "lastnameoncard", "", "text",
                              &field);
    field.autocomplete_attribute = "cc-family-name";
    form->fields.push_back(field);
    field.autocomplete_attribute = "";
  } else {
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  if (use_month_type) {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "month",
                              &field);
    form->fields.push_back(field);
  } else {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("", "ccyear", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form->fields.push_back(field);
}

FormData CreateTestIbanFormData(std::string_view value) {
  FormData form;
  CreateTestIbanFormData(&form, value);
  return form;
}

void CreateTestIbanFormData(FormData* form_data, std::string_view value) {
  FormFieldData field;
  test::CreateTestFormField("IBAN Value:", "iban_value", value, "text", &field);
  form_data->fields.push_back(field);
}

}  // namespace autofill::test
