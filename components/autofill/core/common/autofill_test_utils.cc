// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_test_utils.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill::test {

namespace {

FormData ConstructFormWithNameRenderIdAndProtocol(bool is_https) {
  FormData form;
  form.set_name(u"MyForm");
  form.set_renderer_id(MakeFormRendererId());
  std::string_view protocol = is_https ? "https://" : "http://";
  form.set_url(GURL(base::StrCat({protocol, "myform.com/form.html"})));
  form.set_action(GURL(base::StrCat({protocol, "myform.com/submit.html"})));
  form.set_main_frame_origin(url::Origin::Create(
      GURL(base::StrCat({protocol, "myform_root.com/form.html"}))));
  return form;
}

}  // namespace

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
  form.set_host_frame(frame_token);
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_host_frame(frame_token);
  }
  return form;
}

FormData WithoutValues(FormData form) {
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_value({});
  }
  return form;
}

FormData AsAutofilled(FormData form, bool is_autofilled) {
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_is_autofilled(is_autofilled);
  }
  return form;
}

FormData WithoutUnserializedData(FormData form) {
  form.set_url({});
  form.set_main_frame_origin({});
  form.set_host_frame({});
  for (FormFieldData& field : test_api(form).fields()) {
    field = WithoutUnserializedData(std::move(field));
  }
  return form;
}

FormFieldData WithoutUnserializedData(FormFieldData field) {
  field.set_host_frame({});
  return field;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  FormControlType type) {
  FormFieldData field;
  field.set_host_frame(MakeLocalFrameToken());
  field.set_renderer_id(MakeFieldRendererId());
  field.set_label(base::UTF8ToUTF16(label));
  field.set_name(base::UTF8ToUTF16(name));
  field.set_value(base::UTF8ToUTF16(value));
  field.set_form_control_type(type);
  field.set_is_focusable(true);
  return field;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  FormControlType type,
                                  std::string_view autocomplete) {
  FormFieldData field = CreateTestFormField(label, name, value, type);
  field.set_autocomplete_attribute(std::string(autocomplete));
  field.set_parsed_autocomplete(ParseAutocompleteAttribute(autocomplete));
  return field;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  FormControlType type,
                                  std::string_view autocomplete,
                                  uint64_t max_length) {
  FormFieldData field = CreateTestFormField(label, name, value, type);
  // First, set the `max_length`, as the `parsed_autocomplete` is set based on
  // this value.
  field.set_max_length(max_length);
  field.set_autocomplete_attribute(std::string(autocomplete));
  field.set_parsed_autocomplete(ParseAutocompleteAttribute(autocomplete));
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
  return CreateTestSelectField(label, name, value, autocomplete, values,
                               contents,
                               /*type=*/FormControlType::kSelectOne);
}

FormFieldData CreateTestSelectField(const std::vector<const char*>& values) {
  return CreateTestSelectField(/*label=*/"", /*name=*/"", /*value=*/"",
                               /*autocomplete=*/"", values,
                               /*contents=*/values);
}

FormFieldData CreateTestSelectField(std::string_view label,
                                    std::string_view name,
                                    std::string_view value,
                                    std::string_view autocomplete,
                                    const std::vector<const char*>& values,
                                    const std::vector<const char*>& contents,
                                    FormControlType type) {
  CHECK(type == FormControlType::kSelectOne);
  FormFieldData field = CreateTestFormField(label, name, value, type);
  field.set_autocomplete_attribute(std::string(autocomplete));
  field.set_parsed_autocomplete(ParseAutocompleteAttribute(autocomplete));

  CHECK_EQ(values.size(), contents.size());
  std::vector<SelectOption> options;
  options.reserve(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    options.push_back({
        .value = base::UTF8ToUTF16(values[i]),
        .text = base::UTF8ToUTF16(contents[i]),
    });
  }
  field.set_options(std::move(options));
  return field;
}

FormFieldData CreateTestDatalistField(std::string_view label,
                                      std::string_view name,
                                      std::string_view value,
                                      const std::vector<const char*>& values,
                                      const std::vector<const char*>& labels) {
  // Fill the base attributes.
  FormFieldData field =
      CreateTestFormField(label, name, value, FormControlType::kInputText);
  std::vector<SelectOption> datalist_options;
  datalist_options.reserve(std::min(values.size(), labels.size()));
  for (size_t i = 0; i < std::min(values.size(), labels.size()); ++i) {
    datalist_options.push_back({.value = base::UTF8ToUTF16(values[i]),
                                .text = base::UTF8ToUTF16(labels[i])});
  }
  field.set_datalist_options(std::move(datalist_options));
  return field;
}

FormData CreateTestPersonalInformationFormData() {
  FormData form = ConstructFormWithNameRenderIdAndProtocol(/*is_https=*/true);
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Middle Name", "middlename", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Email", "email", "",
                                       FormControlType::kInputEmail)});
  return form;
}

FormData CreateTestCreditCardFormData(bool is_https,
                                      bool use_month_type,
                                      bool split_names) {
  FormData form = ConstructFormWithNameRenderIdAndProtocol(is_https);

  if (split_names) {
    test_api(form).Append(
        CreateTestFormField("First Name on Card", "firstnameoncard", "",
                            FormControlType::kInputText, "cc-given-name"));
    test_api(form).Append(
        CreateTestFormField("Last Name on Card", "lastnameoncard", "",
                            FormControlType::kInputText, "cc-family=name"));
  } else {
    test_api(form).Append(CreateTestFormField("Name on Card", "nameoncard", "",
                                              FormControlType::kInputText));
  }
  test_api(form).Append(CreateTestFormField("Card Number", "cardnumber", "",
                                            FormControlType::kInputText));
  if (use_month_type) {
    test_api(form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                              FormControlType::kInputMonth));
  } else {
    test_api(form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                              FormControlType::kInputText));
    test_api(form).Append(
        CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
  }
  test_api(form).Append(
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));
  return form;
}

FormData CreateTestIbanFormData(std::string_view value, bool is_https) {
  FormData form = ConstructFormWithNameRenderIdAndProtocol(is_https);
  form.set_fields({CreateTestFormField("IBAN Value:", "iban_value", value,
                                       FormControlType::kInputText)});
  return form;
}

FormData CreateTestPasswordFormData() {
  std::vector<FormFieldData> fields;
  fields.push_back(
      CreateTestFormField(/*label=*/"Username:", /*name=*/"username",
                          /*value=*/"", FormControlType::kInputText));
  fields.push_back(
      CreateTestFormField(/*label=*/"Password:", /*name=*/"password",
                          /*value=*/"", FormControlType::kInputPassword));
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  return form;
}

FormData CreateTestUnclassifiedFormData() {
  FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields({CreateTestFormField(
      "unclassifiable label", "unclassifiable name", "unclassifiable value",
      FormControlType::kInputText)});
  return form;
}

}  // namespace autofill::test
