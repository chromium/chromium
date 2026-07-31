// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_test_utils.h"

#include <algorithm>
#include <ranges>
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
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill::test {

namespace {

FormData ConstructFormWithNameRenderIdAndProtocol(
    bool is_https,
    std::vector<CommonFieldDescription> fields = {}) {
  std::string_view protocol = is_https ? "https://" : "http://";
  return GetFormData(
      {.fields = std::move(fields),
       .url = base::StrCat({protocol, "myform.com/form.html"}),
       .action = base::StrCat({protocol, "myform.com/submit.html"}),
       .main_frame_origin = url::Origin::Create(
           GURL(base::StrCat({protocol, "myform_root.com/form.html"})))});
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
        features::debug::kAutofillServerCommunication);
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
    field.set_user_input({});
    field.set_value({});
    field.set_is_autofilled_according_to_renderer(false);
    field.set_check_status(FormFieldData::CheckStatus::kNotCheckable);
    field.set_properties_mask(0);
  }
  return form;
}

FormData WithoutUnserializedData(FormData form) {
  form.set_url({});
  form.set_full_url({});
  form.set_main_frame_origin(
      url::Origin::CreateFromNormalizedTuple("http", "placeholder", 80));
  form.set_host_frame({});
  for (FormFieldData& field : test_api(form).fields()) {
    field = WithoutUnserializedData(std::move(field));
  }
  return form;
}

FormFieldData WithoutUnserializedData(FormFieldData field) {
  field.set_host_frame({});
  field.set_host_form_signature({});
  field.set_origin(
      url::Origin::CreateFromNormalizedTuple("http", "placeholder", 80));
  return field;
}

FormFieldData CreateTestFormField(std::string_view label,
                                  std::string_view name,
                                  std::string_view value,
                                  FormControlType type) {
  return CreateTestFormField(base::UTF8ToUTF16(label), base::UTF8ToUTF16(name),
                             base::UTF8ToUTF16(value), type);
}

FormFieldData CreateTestFormField(std::u16string_view label,
                                  std::u16string_view name,
                                  std::u16string_view value,
                                  FormControlType type) {
  return GetFormFieldData({
      .host_frame = MakeLocalFrameToken(),
      .label = std::u16string(label),
      .name = std::u16string(name),
      .value = std::u16string(value),
      .form_control_type = type,
  });
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
  field.set_max_length(0);

  std::vector<SelectOption> options;
  options.reserve(values.size());
  for (const auto [option_value, option_content] :
       std::views::zip(values, contents)) {
    options.push_back({
        .value = base::UTF8ToUTF16(option_value),
        .text = base::UTF8ToUTF16(option_content),
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
  datalist_options.reserve(values.size());
  for (auto [entry_value, entry_label] : std::views::zip(values, labels)) {
    datalist_options.push_back({.value = base::UTF8ToUTF16(entry_value),
                                .text = base::UTF8ToUTF16(entry_label)});
  }
  field.set_datalist_options(std::move(datalist_options));
  return field;
}

FormData CreateTestPersonalInformationFormData() {
  return ConstructFormWithNameRenderIdAndProtocol(
      /*is_https=*/true, {{.label = u"First Name", .name = u"firstname"},
                          {.label = u"Middle Name", .name = u"middlename"},
                          {.label = u"Last Name", .name = u"lastname"},
                          {.label = u"Email",
                           .name = u"email",
                           .form_control_type = FormControlType::kInputEmail}});
}

FormData CreateTestCreditCardFormData(bool is_https,
                                      bool use_month_type,
                                      bool split_names) {
  std::vector<CommonFieldDescription> fields;
  if (split_names) {
    fields.push_back({.label = u"First Name on Card",
                      .name = u"firstnameoncard",
                      .autocomplete_attribute = "cc-given-name"});
    fields.push_back({.label = u"Last Name on Card",
                      .name = u"lastnameoncard",
                      .autocomplete_attribute = "cc-family-name"});
  } else {
    fields.push_back({.label = u"Name on Card",
                      .name = u"nameoncard",
                      .autocomplete_attribute = "cc-name"});
  }
  fields.push_back({.label = u"Card Number",
                    .name = u"cardnumber",
                    .autocomplete_attribute = "cc-number"});
  if (use_month_type) {
    fields.push_back({.label = u"Expiration Date",
                      .name = u"ccmonth",
                      .autocomplete_attribute = "cc-exp",
                      .form_control_type = FormControlType::kInputMonth});
  } else {
    fields.push_back({.label = u"Expiration Month",
                      .name = u"ccmonth",
                      .autocomplete_attribute = "cc-exp-month"});
    fields.push_back({.label = u"Expiration Year",
                      .name = u"ccyear",
                      .autocomplete_attribute = "cc-exp-year"});
  }
  fields.push_back({.label = u"CVC", .name = u"cvc"});
  return ConstructFormWithNameRenderIdAndProtocol(is_https, std::move(fields));
}

FormData CreateTestIbanFormData(std::string_view value, bool is_https) {
  return ConstructFormWithNameRenderIdAndProtocol(
      is_https, {{.label = u"IBAN Value:",
                  .name = u"iban_value",
                  .value = base::UTF8ToUTF16(value)}});
}

FormData CreateTestLoyaltyCardFormData() {
  return ConstructFormWithNameRenderIdAndProtocol(
      /*is_https=*/true,
      {{.label = u"Your loyalty card:", .name = u"loyalty_card"}});
}

FormData CreateTestEmailOrLoyaltyCardFormData() {
  return ConstructFormWithNameRenderIdAndProtocol(
      /*is_https=*/true, {{.label = u"Email or member number:",
                           .name = u"email_or_member_number"}});
}

FormData CreateTestMerchantPromoCodeFormData() {
  return ConstructFormWithNameRenderIdAndProtocol(
      /*is_https=*/true, {{.label = u"Promo code", .name = u"promocode"}});
}

FormData CreateTestPasswordFormData() {
  return GetFormData({
      .fields = {{.label = u"Username:", .name = u"username"},
                 {.label = u"Password:",
                  .name = u"password",
                  .form_control_type = FormControlType::kInputPassword}},
      .url = "https://www.foo.com",
      .action = "",
      .main_frame_origin = url::Origin::Create(GURL("https://www.foo.com")),
  });
}

[[nodiscard]] FormData CreateTestSignupFormData() {
  return GetFormData({
      .fields = {{.label = u"Username:", .name = u"username"},
                 {.label = u"Password:",
                  .name = u"password",
                  .form_control_type = FormControlType::kInputPassword},
                 {.label = u"Password (confirm)",
                  .name = u"password_2",
                  .form_control_type = FormControlType::kInputPassword}},
      .url = "https://www.foo.com",
      .action = "",
      .main_frame_origin = url::Origin::Create(GURL("https://www.foo.com")),
  });
}

FormData CreateTestUnclassifiedFormData() {
  return GetFormData({
      .fields = {{.label = u"unclassifiable label",
                  .name = u"unclassifiable name",
                  .value = u"unclassifiable value"}},
      .url = "https://www.foo.com",
      .action = "",
      .main_frame_origin = url::Origin::Create(GURL("https://www.foo.com")),
  });
}

#define FFD_PROPERTY_EQ(property) \
  testing::Property(#property, &FormFieldData::property, expected.property())

testing::Matcher<FormFieldData> FormFieldDataEq(const FormFieldData& expected) {
  return testing::AllOf(
      // Keep in the same order as FormFieldDataMembers in form_field_data.h.
      // LINT.IfChange(FormFieldDataEq)
      // clang-format off
      FFD_PROPERTY_EQ(name),
      FFD_PROPERTY_EQ(id_attribute),
      FFD_PROPERTY_EQ(name_attribute),
      FFD_PROPERTY_EQ(label),
      FFD_PROPERTY_EQ(value),
      FFD_PROPERTY_EQ(selected_option_text),
      FFD_PROPERTY_EQ(selected_text),
      FFD_PROPERTY_EQ(form_control_type),
      FFD_PROPERTY_EQ(autocomplete_attribute),
      FFD_PROPERTY_EQ(parsed_autocomplete),
      FFD_PROPERTY_EQ(pattern),
      FFD_PROPERTY_EQ(placeholder),
      FFD_PROPERTY_EQ(placeholder_attribute),
      FFD_PROPERTY_EQ(css_classes),
      FFD_PROPERTY_EQ(aria_label),
      FFD_PROPERTY_EQ(aria_description),
      FFD_PROPERTY_EQ(nonce),
      FFD_PROPERTY_EQ(host_frame),
      FFD_PROPERTY_EQ(renderer_id),
      FFD_PROPERTY_EQ(host_form_id),
      FFD_PROPERTY_EQ(host_form_signature),
      FFD_PROPERTY_EQ(origin),
      FFD_PROPERTY_EQ(form_control_ax_id),
      FFD_PROPERTY_EQ(max_length),
      FFD_PROPERTY_EQ(is_autofilled_according_to_renderer),
      FFD_PROPERTY_EQ(check_status),
      FFD_PROPERTY_EQ(is_focusable),
      FFD_PROPERTY_EQ(is_visible),
      FFD_PROPERTY_EQ(should_autocomplete),
      FFD_PROPERTY_EQ(role),
      FFD_PROPERTY_EQ(text_direction),
      FFD_PROPERTY_EQ(properties_mask),
      FFD_PROPERTY_EQ(is_enabled),
      FFD_PROPERTY_EQ(is_readonly),
      FFD_PROPERTY_EQ(user_input),
      FFD_PROPERTY_EQ(allows_writing_suggestions),
      FFD_PROPERTY_EQ(options),
      FFD_PROPERTY_EQ(label_source),
      FFD_PROPERTY_EQ(bounds),
      FFD_PROPERTY_EQ(datalist_options),
      FFD_PROPERTY_EQ(force_override),
      // clang-format on
      // LINT.ThenChange(form_field_data.h:FormFieldDataMembers)
      // Backstop for members compared by IdenticalAndEquivalentDomElements().
      testing::Truly([expected](const FormFieldData& actual) {
        return FormFieldData::IdenticalAndEquivalentDomElements(expected,
                                                                actual);
      }));
}

#undef FFD_PROPERTY_EQ

#define FD_PROPERTY_EQ(property) \
  testing::Property(#property, &FormData::property, expected.property())

testing::Matcher<FormData> FormDataEq(const FormData& expected) {
  // Build field matchers for each field in the expected form.
  std::vector<testing::Matcher<FormFieldData>> field_matchers;
  field_matchers.reserve(expected.fields().size());
  for (const FormFieldData& field : expected.fields()) {
    field_matchers.push_back(FormFieldDataEq(field));
  }

  return testing::AllOf(
      // Keep in the same order as FormDataMembers in form_data.h.
      // LINT.IfChange(FormDataEq)
      // clang-format off
      FD_PROPERTY_EQ(host_frame),
      FD_PROPERTY_EQ(renderer_id),
      FD_PROPERTY_EQ(child_frames),
      // Compare fields_ recursively with FormFieldDataEq().
      testing::Property("fields", &FormData::fields,
                        testing::ElementsAreArray(field_matchers)),
      FD_PROPERTY_EQ(id_attribute),
      FD_PROPERTY_EQ(name_attribute),
      FD_PROPERTY_EQ(name),
      FD_PROPERTY_EQ(button_titles),
      FD_PROPERTY_EQ(url),
      FD_PROPERTY_EQ(full_url),
      FD_PROPERTY_EQ(action),
      FD_PROPERTY_EQ(is_action_empty),
      FD_PROPERTY_EQ(main_frame_origin),
      FD_PROPERTY_EQ(submission_event),
      FD_PROPERTY_EQ(username_predictions),
      FD_PROPERTY_EQ(is_gaia_with_skip_save_password_form),
      FD_PROPERTY_EQ(likely_contains_captcha),
      FD_PROPERTY_EQ(version),
      // clang-format on
      // LINT.ThenChange(form_data.h:FormDataMembers)
      // Backstop for members compared by IdenticalAndEquivalentDomElements().
      testing::Truly([expected](const FormData& actual) {
        return FormData::IdenticalAndEquivalentDomElements(expected, actual);
      }));
}

#undef FD_PROPERTY_EQ

}  // namespace autofill::test
