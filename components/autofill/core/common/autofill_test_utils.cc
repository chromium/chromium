// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_test_utils.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
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

RemoteFrameToken AutofillTestEnvironment::NextRemoteFrameToken() {
  return RemoteFrameToken(base::UnguessableToken::CreateForTesting(
      ++remote_frame_token_counter_high_, ++remote_frame_token_counter_low_));
}

FormRendererId AutofillTestEnvironment::NextFormRendererId() {
  return FormRendererId(++form_renderer_id_counter_);
}

FieldRendererId AutofillTestEnvironment::NextFieldRendererId() {
  return FieldRendererId(++field_renderer_id_counter_);
}

AutofillUnitTestEnvironment::AutofillUnitTestEnvironment(const Options& options)
    : AutofillTestEnvironment(options) {}

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

RemoteFrameToken MakeRemoteFrameToken(RandomizeFrame randomize) {
  if (*randomize) {
    return RemoteFrameToken(
        AutofillTestEnvironment::GetCurrent().NextRemoteFrameToken());
  } else {
    return RemoteFrameToken(
        base::UnguessableToken::CreateForTesting(12345, 67890));
  }
}

FormData CreateFormDataForFrame(FormData form, LocalFrameToken frame_token) {
  form.host_frame = frame_token;
  for (FormFieldData& field : form.fields) {
    field.host_frame = frame_token;
  }
  return form;
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
  field.host_frame = MakeLocalFrameToken();
  field.unique_renderer_id = MakeFieldRendererId();
  field.label = base::UTF8ToUTF16(label);
  field.name = base::UTF8ToUTF16(name);
  field.value = base::UTF8ToUTF16(value);
  field.form_control_type = StringToFormControlType(type);
  field.is_focusable = true;
  return field;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  std::string_view type,
                                  std::string_view autocomplete) {
  FormFieldData field = CreateTestFormField(label, name, value, type);
  field.autocomplete_attribute = autocomplete;
  field.parsed_autocomplete = ParseAutocompleteAttribute(autocomplete);
  return field;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  std::string_view type,
                                  std::string_view autocomplete,
                                  uint64_t max_length) {
  FormFieldData field = CreateTestFormField(label, name, value, type);
  // First, set the `max_length`, as the `parsed_autocomplete` is set based on
  // this value.
  field.max_length = max_length;
  field.autocomplete_attribute = autocomplete;
  field.parsed_autocomplete = ParseAutocompleteAttribute(autocomplete);
  return field;
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
  return CreateTestSelectOrSelectListField(label, name, value, autocomplete,
                                           values, contents,
                                           /*field_type=*/"select-one");
}

FormFieldData CreateTestSelectField(const std::vector<const char*>& values) {
  return CreateTestSelectField(/*label=*/"", /*name=*/"", /*value=*/"",
                               /*autocomplete=*/"", values,
                               /*contents=*/values);
}

FormFieldData CreateTestSelectOrSelectListField(
    std::string_view label,
    std::string_view name,
    std::string_view value,
    std::string_view autocomplete,
    const std::vector<const char*>& values,
    const std::vector<const char*>& contents,
    std::string_view field_type) {
  CHECK(field_type == "select-one" || field_type == "selectlist");
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
  form.unique_renderer_id = MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.fields = {CreateTestFormField("First Name", "firstname", "", "text"),
                 CreateTestFormField("Middle Name", "middlename", "", "text"),
                 CreateTestFormField("Last Name", "lastname", "", "text"),
                 CreateTestFormField("Email", "email", "", "email")};
  return form;
}

FormData CreateTestCreditCardFormData(bool is_https,
                                      bool use_month_type,
                                      bool split_names) {
  FormData form;
  form.unique_renderer_id = MakeFormRendererId();
  form.name = u"MyForm";
  if (is_https) {
    form.url = GURL("https://myform.com/form.html");
    form.action = GURL("https://myform.com/submit.html");
    form.main_frame_origin =
        url::Origin::Create(GURL("https://myform_root.com/form.html"));
  } else {
    form.url = GURL("http://myform.com/form.html");
    form.action = GURL("http://myform.com/submit.html");
    form.main_frame_origin =
        url::Origin::Create(GURL("http://myform_root.com/form.html"));
  }

  if (split_names) {
    form.fields.push_back(CreateTestFormField(
        "First Name on Card", "firstnameoncard", "", "text", "cc-given-name"));
    form.fields.push_back(CreateTestFormField(
        "Last Name on Card", "lastnameoncard", "", "text", "cc-family=name"));
  } else {
    form.fields.push_back(
        CreateTestFormField("Name on Card", "nameoncard", "", "text"));
  }
  form.fields.push_back(
      CreateTestFormField("Card Number", "cardnumber", "", "text"));
  if (use_month_type) {
    form.fields.push_back(
        CreateTestFormField("Expiration Date", "ccmonth", "", "month"));
  } else {
    form.fields.push_back(
        CreateTestFormField("Expiration Date", "ccmonth", "", "text"));
    form.fields.push_back(CreateTestFormField("", "ccyear", "", "text"));
  }
  form.fields.push_back(CreateTestFormField("CVC", "cvc", "", "text"));
  return form;
}

FormData CreateTestIbanFormData(std::string_view value) {
  FormData form;
  form.url = GURL("https://www.foo.com");
  form.fields = {
      CreateTestFormField("IBAN Value:", "iban_value", value, "text")};
  return form;
}

}  // namespace autofill::test
