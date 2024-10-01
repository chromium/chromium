// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::autofill::FormControlType;
using ::autofill::test::CreateTestFormField;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::ResultOf;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

class FormStructureTestImpl : public test::FormStructureTest {
 public:
  static std::string Hash64Bit(const std::string& str) {
    return base::NumberToString(StrToHash64Bit(str));
  }

 protected:
  bool FormIsAutofillable(const FormData& form) {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    return form_structure.IsAutofillable();
  }

  bool FormShouldRunHeuristics(const FormData& form) {
    return FormStructure(form).ShouldRunHeuristics();
  }

  bool FormShouldRunHeuristicsForSingleFieldForms(const FormData& form) {
    return FormStructure(form).ShouldRunHeuristicsForSingleFieldForms();
  }

  bool FormShouldBeQueried(const FormData& form) {
    return FormStructure(form).ShouldBeQueried();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(FormStructureTestImpl, FieldCount) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "FieldCount",
         .fields = {{.role = FieldType::USERNAME},
                    {.label = u"Password",
                     .name = u"password",
                     .form_control_type = FormControlType::kInputPassword},
                    {.label = u"Submit",
                     .name = u"",
                     .form_control_type = FormControlType::kInputText},
                    {.label = u"address1",
                     .name = u"address1",
                     .should_autocomplete = false}}},
        {
            .determine_heuristic_type = true,
            .field_count = 4,
        },
        {}}});
}

TEST_F(FormStructureTestImpl, AutofillCount) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "AutofillCount",
         .fields = {{.role = FieldType::USERNAME},
                    {.label = u"Password",
                     .name = u"password",
                     .form_control_type = FormControlType::kInputPassword},
                    {.role = FieldType::EMAIL_ADDRESS},
                    {.role = FieldType::ADDRESS_HOME_CITY},
                    {.role = FieldType::ADDRESS_HOME_STATE,
                     .form_control_type = FormControlType::kSelectOne},
                    {.label = u"Submit",
                     .name = u"",
                     .form_control_type = FormControlType::kInputText}}},
        {
            .determine_heuristic_type = true,
            .autofill_count = 3,
        },
        {}},

       {{.description_for_logging = "AutofillCountWithNonFillableField",
         .fields = {{.role = FieldType::USERNAME},
                    {.label = u"Password",
                     .name = u"password",
                     .form_control_type = FormControlType::kInputPassword},
                    {.role = FieldType::EMAIL_ADDRESS},
                    {.role = FieldType::ADDRESS_HOME_CITY},
                    {.role = FieldType::ADDRESS_HOME_STATE,
                     .form_control_type = FormControlType::kSelectOne},
                    {.label = u"Submit",
                     .name = u"",
                     .form_control_type = FormControlType::kInputText},
                    {.label = u"address1",
                     .name = u"address1",
                     .should_autocomplete = false}}},
        {
            .determine_heuristic_type = true,
            .autofill_count = 4,
        },
        {}}});
}

TEST_F(FormStructureTestImpl, SourceURL) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  FormStructure form_structure(form);

  EXPECT_EQ(form.url(), form_structure.source_url());
}

TEST_F(FormStructureTestImpl, FullSourceURLWithHashAndParam) {
  FormData form;
  form.set_full_url(GURL("https://www.foo.com/?login=asdf#hash"));
  FormStructure form_structure(form);

  EXPECT_EQ(form.full_url(), form_structure.full_source_url());
}

TEST_F(FormStructureTestImpl, IsAutofillable) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  FormFieldData field;

  // Start with a username field. It should be picked up by the password but
  // not by autofill.
  field.set_label(u"username");
  field.set_name(u"username");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  // With min required fields enabled.
  EXPECT_FALSE(FormIsAutofillable(form));

  // Add a password field. The form should be picked up by the password but
  // not by autofill.
  field.set_label(u"password");
  field.set_name(u"password");
  field.set_form_control_type(FormControlType::kInputPassword);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.set_label(u"Full Name");
  field.set_name(u"fullname");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.set_label(u"Address Line 1");
  field.set_name(u"address1");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // We now have three auto-fillable fields. It's always autofillable.
  field.set_label(u"Email");
  field.set_name(u"email");
  field.set_form_control_type(FormControlType::kInputEmail);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_TRUE(FormIsAutofillable(form));

  // The target cannot include http(s)://*/search...
  form.set_action(GURL("http://google.com/search?q=hello"));

  EXPECT_FALSE(FormIsAutofillable(form));

  // But search can be in the URL.
  form.set_action(GURL("http://search.com/?q=hello"));

  EXPECT_TRUE(FormIsAutofillable(form));
}

class FormStructureTestImpl_ShouldBeParsed_Test : public FormStructureTestImpl {
 public:
  FormStructureTestImpl_ShouldBeParsed_Test() {
    form_.set_url(GURL("http://www.foo.com/"));
    form_structure_ = std::make_unique<FormStructure>(form_);
  }

  ~FormStructureTestImpl_ShouldBeParsed_Test() override = default;

  void SetAction(GURL action) {
    form_.set_action(action);
    form_structure_ = nullptr;
  }

  void AddField(FormFieldData field) {
    field.set_renderer_id(test::MakeFieldRendererId());
    test_api(form_).Append(std::move(field));
    form_structure_ = nullptr;
  }

  void AddTextField() {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputText);
    AddField(field);
  }

  FormStructure& form_structure() {
    if (!form_structure_) {
      form_structure_ = std::make_unique<FormStructure>(form_);
    }
    return *form_structure_.get();
  }

 private:
  FormData form_;
  std::unique_ptr<FormStructure> form_structure_;
};

// Empty forms should not be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, FalseIfNoFields) {
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
}

// Forms with only checkable fields should not be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, IgnoresCheckableFields) {
  // Start with a single checkable field.
  {
    FormFieldData field;
    field.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
    field.set_form_control_type(FormControlType::kInputRadio);
    AddField(field);
  }
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));

  // Add a second checkable field.
  {
    FormFieldData field;
    field.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
    field.set_form_control_type(FormControlType::kInputCheckbox);
    AddField(field);
  }
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));

  // Add one text field.
  AddTextField();
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
}

// Forms with at least one text field should be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, TrueIfOneTextField) {
  AddTextField();
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));

  AddTextField();
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));
}

// Forms that have only select fields should not be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, FalseIfOnlySelectField) {
  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kSelectOne);
    AddField(field);
  }
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));

  AddTextField();
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));
}

// Form whose action is a search URL should not be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, FalseIfSearchURL) {
  AddTextField();
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));

  // The target cannot include http(s)://*/search...
  SetAction(GURL("http://google.com/search?q=hello"));
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));

  // But search can be in the URL.
  SetAction(GURL("http://search.com/?q=hello"));
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
}

// Forms with two password fields and no other fields should be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, TrueIfOnlyPasswordFields) {
  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputPassword);
    AddField(field);
  }
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure())
          .ShouldBeParsed(
              {.min_required_fields = 2,
               .required_fields_for_forms_with_only_password_fields = 1}));
  EXPECT_FALSE(
      test_api(form_structure())
          .ShouldBeParsed(
              {.min_required_fields = 2,
               .required_fields_for_forms_with_only_password_fields = 2}));

  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputPassword);
    AddField(field);
  }
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure())
          .ShouldBeParsed(
              {.min_required_fields = 2,
               .required_fields_for_forms_with_only_password_fields = 1}));
  EXPECT_TRUE(
      test_api(form_structure())
          .ShouldBeParsed(
              {.min_required_fields = 2,
               .required_fields_for_forms_with_only_password_fields = 2}));
}

// Forms with at least one field with an autocomplete attribute should be
// parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test,
       TrueIfOneFieldHasAutocomplete) {
  AddTextField();
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));

  {
    FormFieldData field;
    field.set_parsed_autocomplete(AutocompleteParsingResult{
        .section = "my-billing-section", .field_type = HtmlFieldType::kName});
    field.set_form_control_type(FormControlType::kInputText);
    AddField(field);
  }
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));
}

TEST_F(FormStructureTestImpl, ShouldBeParsed_BadScheme) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_fields(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText,
                           "name"),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText,
                           "email"),
       CreateTestFormField("Address", "address", "",
                           FormControlType::kInputText, "address-line1")});

  // Baseline, HTTP should work.
  form.set_url(GURL("http://wwww.foo.com/myform"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Baseline, HTTPS should work.
  form.set_url(GURL("https://wwww.foo.com/myform"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Chrome internal urls shouldn't be parsed.
  form.set_url(GURL("chrome://settings"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // FTP urls shouldn't be parsed.
  form.set_url(GURL("ftp://ftp.foo.com/form.html"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // Blob urls shouldn't be parsed.
  form.set_url(GURL("blob://blob.foo.com/form.html"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // About urls shouldn't be parsed.
  form.set_url(GURL("about://about.foo.com/form.html"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());
}

// Tests that ShouldBeParsed returns true for a form containing less than three
// fields if at least one has an autocomplete attribute.
TEST_F(FormStructureTestImpl, ShouldBeParsed_TwoFields_HasAutocomplete) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_fields({CreateTestFormField("Name", "name", "",
                                       FormControlType::kInputText, "name"),
                   CreateTestFormField("Address", "Address", "",
                                       FormControlType::kSelectOne, "")});
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(form_structure->ShouldBeParsed());
}

// Tests that ShouldBeParsed returns true for a form containing less than three
// fields if at least one has an autocomplete attribute.
TEST_F(FormStructureTestImpl, DetermineHeuristicTypes_AutocompleteFalse) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "DetermineHeuristicTypes_AutocompleteFalse",
         .fields =
             {{.label = u"Name",
               .name = u"name",
               .autocomplete_attribute = "false",
               .parsed_autocomplete = ParseAutocompleteAttribute("false")},
              {.role = FieldType::EMAIL_ADDRESS,
               .autocomplete_attribute = "false",
               .parsed_autocomplete = ParseAutocompleteAttribute("false")},
              {.role = FieldType::ADDRESS_HOME_STATE,
               .autocomplete_attribute = "false",
               .parsed_autocomplete = ParseAutocompleteAttribute("false"),
               .form_control_type = FormControlType::kSelectOne}}},
        {
            .determine_heuristic_type = true,
            .should_be_parsed = true,
            .autofill_count = 3,
        },
        {.expected_overall_type = {NAME_FULL, EMAIL_ADDRESS,
                                   ADDRESS_HOME_STATE}}}});
}

TEST_F(FormStructureTestImpl, HeuristicsContactInfo) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "HeuristicsContactInfo",
         .fields = {{.role = FieldType::NAME_FIRST},
                    {.role = FieldType::NAME_LAST},
                    {.role = FieldType::EMAIL_ADDRESS},
                    {.role = FieldType::PHONE_HOME_NUMBER},
                    {.label = u"Ext:", .name = u"phoneextension"},
                    {.label = u"Address", .name = u"address"},
                    {.role = FieldType::ADDRESS_HOME_CITY},
                    {.role = FieldType::ADDRESS_HOME_ZIP},
                    {.label = u"Submit",
                     .name = u"",
                     .form_control_type = FormControlType::kInputText}}},
        {
            .determine_heuristic_type = true,
            .field_count = 9,
            .autofill_count = 8,
        },
        {.expected_heuristic_type = {
             NAME_FIRST, NAME_LAST, EMAIL_ADDRESS, PHONE_HOME_CITY_AND_NUMBER,
             PHONE_HOME_EXTENSION, ADDRESS_HOME_LINE1, ADDRESS_HOME_CITY,
             ADDRESS_HOME_ZIP, UNKNOWN_TYPE}}}});
}

// Verify that we can correctly process the |autocomplete| attribute.
TEST_F(FormStructureTestImpl, HeuristicsAutocompleteAttribute) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "HeuristicsAutocompleteAttribute",
         .fields =
             {{.label = u"",
               .name = u"field1",
               .autocomplete_attribute = "given-name",
               .parsed_autocomplete = ParseAutocompleteAttribute("given-name")},
              {.label = u"",
               .name = u"field2",
               .autocomplete_attribute = "family-name",
               .parsed_autocomplete =
                   ParseAutocompleteAttribute("family-name")},
              {.label = u"",
               .name = u"field3",
               .autocomplete_attribute = "email",
               .parsed_autocomplete = ParseAutocompleteAttribute("email")}}},
        {
            .determine_heuristic_type = true,
            .is_autofillable = true,
            .has_author_specified_types = true,
            .field_count = 3,
            .autofill_count = 3,
        },
        {.expected_html_type = {HtmlFieldType::kGivenName,
                                HtmlFieldType::kFamilyName,
                                HtmlFieldType::kEmail},
         .expected_heuristic_type = {UNKNOWN_TYPE, UNKNOWN_TYPE,
                                     UNKNOWN_TYPE}}}});
}

// All fields share a common prefix which could confuse the heuristics. Test
// that the common prefix is stripped out before running heuristics.
TEST_F(FormStructureTestImpl, StripCommonNamePrefix) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "StripCommonNamePrefix",
         .fields =
             {{.role = FieldType::NAME_FIRST,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$firstname"},
              {.role = FieldType::NAME_LAST,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$lastname"},
              {.role = FieldType::EMAIL_ADDRESS,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$email"},
              {.role = FieldType::PHONE_HOME_NUMBER,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$phone"},
              {.label = u"Submit",
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$submit",
               .form_control_type = FormControlType::kInputText}}},
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .field_count = 5,
         .autofill_count = 4},
        {.expected_heuristic_type = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                     PHONE_HOME_CITY_AND_NUMBER,
                                     UNKNOWN_TYPE}}}});
}

// All fields share a common prefix which is small enough that it is not
// stripped from the name before running the heuristics.
TEST_F(FormStructureTestImpl, StripCommonNamePrefix_SmallPrefix) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "StripCommonNamePrefix_SmallPrefix",
         .fields = {{.label = u"Address 1", .name = u"address1"},
                    {.label = u"Address 2", .name = u"address2"},
                    {.label = u"Address 3", .name = u"address3"}}},
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .field_count = 3,
         .autofill_count = 3},
        {.expected_heuristic_type = {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                                     ADDRESS_HOME_LINE3}}}});
}

TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_Minimal) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_Minimal",
         .fields = {{.role = FieldType::CREDIT_CARD_NUMBER},
                    {.label = u"Expiration", .name = u"cc_exp"},
                    {.role = FieldType::ADDRESS_HOME_ZIP}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = true},
        {}}});
}

TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_Full) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_Full",
         .fields = {{.label = u"Name on Card", .name = u"name_on_card"},
                    {.role = FieldType::CREDIT_CARD_NUMBER},
                    {.label = u"Exp Month", .name = u"ccmonth"},
                    {.label = u"Exp Year", .name = u"ccyear"},
                    {.label = u"Verification", .name = u"verification"},
                    {.label = u"Submit",
                     .name = u"submit",
                     .form_control_type = FormControlType::kInputText}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = true},
        {}}});
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_OnlyCCNumber) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_OnlyCCNumber",
         .fields = {{.role = FieldType::CREDIT_CARD_NUMBER}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = false},
        {}}});
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_AddressForm) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_AddressForm",
         .fields = {{.role = FieldType::NAME_FIRST, .name = u""},
                    {.role = FieldType::NAME_LAST, .name = u""},
                    {.role = FieldType::EMAIL_ADDRESS, .name = u""},
                    {.role = FieldType::PHONE_HOME_NUMBER, .name = u""},
                    {.label = u"Address", .name = u""},
                    {.label = u"Address", .name = u""},
                    {.role = FieldType::ADDRESS_HOME_ZIP, .name = u""}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = false},
        {}}});
}

// Verify that we can correctly process the 'autocomplete' attribute for phone
// number types (especially phone prefixes and suffixes).
TEST_F(FormStructureTestImpl, HeuristicsAutocompleteAttributePhoneTypes) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "HeuristicsAutocompleteAttributePhoneTypes",
         .fields = {{.label = u"",
                     .name = u"field1",
                     .autocomplete_attribute = "tel-local",
                     .parsed_autocomplete =
                         ParseAutocompleteAttribute("tel-local")},
                    {.label = u"",
                     .name = u"field2",
                     .autocomplete_attribute = "tel-local-prefix",
                     .parsed_autocomplete =
                         ParseAutocompleteAttribute("tel-local-prefix")},
                    {.label = u"",
                     .name = u"field3",
                     .autocomplete_attribute = "tel-local-suffix",
                     .parsed_autocomplete =
                         ParseAutocompleteAttribute("tel-local-suffix")}}},
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .field_count = 3,
         .autofill_count = 3},
        {.expected_html_type = {HtmlFieldType::kTelLocal,
                                HtmlFieldType::kTelLocalPrefix,
                                HtmlFieldType::kTelLocalSuffix}}}});
}

// The heuristics and server predictions should run if there are more than two
// fillable fields.
TEST_F(FormStructureTestImpl,
       HeuristicsAndServerPredictions_BigForm_NoAutocompleteAttribute) {
  CheckFormStructureTestData(
      {{{.description_for_logging =
             "HeuristicsAndServerPredictions_BigForm_NoAutocompleteAttribute",
         .fields = {{.role = FieldType::NAME_FIRST},
                    {.role = FieldType::NAME_LAST},
                    {.role = FieldType::EMAIL_ADDRESS}}},
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .should_be_queried = true,
         .should_be_uploaded = true,
         .field_count = 3,
         .autofill_count = 3},
        {.expected_heuristic_type = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS}}}});
}

// The heuristics and server predictions should run even if a valid autocomplete
// attribute is present in the form (if it has more that two fillable fields).
TEST_F(FormStructureTestImpl,
       HeuristicsAndServerPredictions_ValidAutocompleteAttribute) {
  CheckFormStructureTestData(
      {{{.description_for_logging =
             "HeuristicsAndServerPredictions_ValidAutocompleteAttribute",
         .fields = {{.role = FieldType::NAME_FIRST,
                     .autocomplete_attribute = "given-name",
                     .parsed_autocomplete =
                         ParseAutocompleteAttribute("given-name")},
                    {.role = FieldType::NAME_LAST},
                    {.role = FieldType::EMAIL_ADDRESS}}},
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .should_be_queried = true,
         .should_be_uploaded = true,
         .field_count = 3,
         .autofill_count = 3},
        {.expected_heuristic_type = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS}}}});
}

// The heuristics and server predictions should run even if an unrecognized
// autocomplete attribute is present in the form (if it has more than two
// fillable fields).
TEST_F(FormStructureTestImpl,
       HeuristicsAndServerPredictions_UnrecognizedAutocompleteAttribute) {
  CheckFormStructureTestData(
      {{{
            .description_for_logging = "HeuristicsAndServerPredictions_"
                                       "UnrecognizedAutocompleteAttribute",
            .fields = {{.role = FieldType::NAME_FIRST,
                        .autocomplete_attribute = "unrecognized",
                        .parsed_autocomplete =
                            ParseAutocompleteAttribute("unrecognized")},
                       {.label = u"Middle Name", .name = u"middlename"},
                       {.role = FieldType::NAME_LAST},
                       {.role = FieldType::EMAIL_ADDRESS}},
        },
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .should_be_queried = true,
         .field_count = 4,
         .autofill_count = 4},
        {.expected_heuristic_type = {NAME_FIRST, NAME_MIDDLE, NAME_LAST,
                                     EMAIL_ADDRESS}}}});
}

// Tests whether the heuristics and server predictions are run for forms with
// fewer than 3 fields  and no autocomplete attributes.
TEST_F(FormStructureTestImpl,
       HeuristicsAndServerPredictions_SmallForm_NoAutocompleteAttribute) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"First Name");
  field.set_name(u"firstname");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Last Name");
  field.set_name(u"lastname");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_FALSE(FormShouldRunHeuristics(form));

  EXPECT_TRUE(FormShouldBeQueried(form));

  // Default configuration.
  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(2U, form_structure.field_count());
    ASSERT_EQ(0U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(1)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(1)->server_type());
    EXPECT_FALSE(form_structure.IsAutofillable());
  }
}

// Tests the heuristics and server predictions are not run for forms with less
// than 3 fields, if the minimum fields required feature is enforced, even if an
// autocomplete attribute is specified.
TEST_F(FormStructureTestImpl,
       HeuristicsAndServerPredictions_SmallForm_ValidAutocompleteAttribute) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  // Set a valid autocomplete attribute to the first field.
  form.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "")});
  EXPECT_FALSE(FormShouldRunHeuristics(form));
  EXPECT_TRUE(FormShouldBeQueried(form));

  // As a side effect of parsing small forms (if any of the heuristics, query,
  // or upload minimums are disabled, we'll autofill fields with an
  // autocomplete attribute, even if its the only field in the form.
  {
    FormData form_copy = form;
    test_api(form_copy).Remove(-1);
    FormStructure form_structure(form_copy);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(1U, form_structure.field_count());
    ASSERT_EQ(1U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NAME_FIRST, form_structure.field(0)->Type().GetStorableType());
    EXPECT_TRUE(form_structure.IsAutofillable());
  }
}

// Tests that heuristics for single field parseable types are run for forms with
// fewer than 3 fields.
TEST_F(FormStructureTestImpl, PromoCodeHeuristics_SmallForm) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Promo Code");
  field.set_name(u"promocode");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_TRUE(FormShouldRunHeuristicsForSingleFieldForms(form));

  // Default configuration.
  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(1U, form_structure.field_count());
    ASSERT_EQ(1U, form_structure.autofill_count());
    EXPECT_EQ(MERCHANT_PROMO_CODE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_TRUE(form_structure.IsAutofillable());
  }
}

// Even with an 'autocomplete' attribute set, ShouldBeQueried() should
// return true if the structure contains a password field, since there are
// no local heuristics to depend upon in this case. Fields will still not be
// considered autofillable though.
TEST_F(FormStructureTestImpl, PasswordFormShouldBeQueried) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Email", "email", "",
                                       FormControlType::kInputText, "username"),
                   CreateTestFormField("Password", "Password", "",
                                       FormControlType::kInputPassword)});
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                         nullptr);
  EXPECT_TRUE(form_structure.has_password_field());
  EXPECT_TRUE(form_structure.ShouldBeQueried());
  EXPECT_TRUE(form_structure.ShouldBeUploaded());
}

// Verify that we can correctly process a degenerate section listed in the
// |autocomplete| attribute.
TEST_F(FormStructureTestImpl,
       HeuristicsAutocompleteAttributeWithSectionsDegenerate) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_fields(
      {// Some fields will have no section specified.  These fall into the
       // default section.
       CreateTestFormField("", "", "", FormControlType::kInputText, "email"),
       // Specifying "section-" is equivalent to not specifying a section.
       CreateTestFormField("", "", "", FormControlType::kInputText,
                           "section- email"),
       // Invalid tokens should prevent us from setting a section name.
       CreateTestFormField("", "", "", FormControlType::kInputText,
                           "garbage section-foo email"),
       CreateTestFormField("", "", "", FormControlType::kInputText,
                           "garbage section-bar email"),
       CreateTestFormField("", "", "", FormControlType::kInputText,
                           "garbage shipping email"),
       CreateTestFormField("", "", "", FormControlType::kInputText,
                           "garbage billing email")});
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                         nullptr);

  // Expect the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());
  EXPECT_EQ(2U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to the same
  // section.
  std::set<Section> section_names;
  for (size_t i = 0; i < 6; ++i) {
    section_names.insert(form_structure.field(i)->section());
  }
  EXPECT_EQ(1U, section_names.size());
}

// Verify that we can correctly process repeated sections listed in the
// |autocomplete| attribute.
TEST_F(FormStructureTestImpl,
       HeuristicsAutocompleteAttributeWithSectionsRepeated) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_fields({CreateTestFormField("", "", "", FormControlType::kInputText,
                                       "section-foo email"),
                   CreateTestFormField("", "", "", FormControlType::kInputText,
                                       "section-foo address-line1")});
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                         nullptr);

  // Expect the correct number of fields.
  ASSERT_EQ(2U, form_structure.field_count());
  EXPECT_EQ(2U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to the same
  // section.
  std::set<Section> section_names;
  for (size_t i = 0; i < 2; ++i) {
    section_names.insert(form_structure.field(i)->section());
  }
  EXPECT_EQ(1U, section_names.size());
}

TEST_F(FormStructureTestImpl, HeuristicsSample8) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Your First Name:");
  field.set_name(u"bill.first");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Your Last Name:");
  field.set_name(u"bill.last");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Street Address Line 1:");
  field.set_name(u"bill.street1");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Street Address Line 2:");
  field.set_name(u"bill.street2");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"City");
  field.set_name(u"bill.city");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"State (U.S.):");
  field.set_name(u"bill.state");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Zip/Postal Code:");
  field.set_name(u"BillTo.PostalCode");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Country:");
  field.set_name(u"bill.country");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Phone Number:");
  field.set_name(u"BillTo.Phone");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"Submit");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(10U, form_structure->field_count());
  ASSERT_EQ(9U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(2)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(3)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(4)->heuristic_type());
  // State.
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure->field(5)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(6)->heuristic_type());
  // Country.
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, form_structure->field(7)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_CITY_AND_NUMBER,
            form_structure->field(8)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(9)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsSample6) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"E-mail address");
  field.set_name(u"email");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Full name");
  field.set_name(u"name");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Company");
  field.set_name(u"company");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address");
  field.set_name(u"address");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"City");
  field.set_name(u"city");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Zip Code");
  field.set_name(u"Home.PostalCode");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"Submit");
  field.set_value(u"continue");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(7U, form_structure->field_count());
  ASSERT_EQ(6U, form_structure->autofill_count());

  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(0)->heuristic_type());
  // Full name.
  EXPECT_EQ(NAME_FULL, form_structure->field(1)->heuristic_type());
  // Company
  EXPECT_EQ(COMPANY_NAME, form_structure->field(2)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(3)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(4)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(5)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
}

// Tests a sequence of FormFields where only labels are supplied to heuristics
// for matching.  This works because FormFieldData labels are matched in the
// case that input element ids (or |name| fields) are missing.
TEST_F(FormStructureTestImpl, HeuristicsLabelsOnly) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"First Name");
  field.set_name(std::u16string());
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Last Name");
  field.set_name(std::u16string());
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Email");
  field.set_name(std::u16string());
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Phone");
  field.set_name(std::u16string());
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address");
  field.set_name(std::u16string());
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address");
  field.set_name(std::u16string());
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Zip code");
  field.set_name(std::u16string());
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"Submit");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(8U, form_structure->field_count());
  ASSERT_EQ(7U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_CITY_AND_NUMBER,
            form_structure->field(3)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(4)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(5)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(6)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(7)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsCreditCardInfo) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Name on Card");
  field.set_name(u"name_on_card");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Card Number");
  field.set_name(u"card_number");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Exp Month");
  field.set_name(u"ccmonth");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Exp Year");
  field.set_name(u"ccyear");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Verification");
  field.set_name(u"verification");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"Submit");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(6U, form_structure->field_count());
  ASSERT_EQ(5U, form_structure->autofill_count());

  // Credit card name.
  EXPECT_EQ(CREDIT_CARD_NAME_FULL, form_structure->field(0)->heuristic_type());
  // Credit card number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(1)->heuristic_type());
  // Credit card expiration month.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(2)->heuristic_type());
  // Credit card expiration year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(3)->heuristic_type());
  // CVV.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(4)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(5)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsCreditCardInfoWithUnknownCardField) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Name on Card");
  field.set_name(u"name_on_card");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  // This is not a field we know how to process.  But we should skip over it
  // and process the other fields in the card block.
  field.set_label(u"Card image");
  field.set_name(u"card_image");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Card Number");
  field.set_name(u"card_number");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Exp Month");
  field.set_name(u"ccmonth");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Exp Year");
  field.set_name(u"ccyear");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Verification");
  field.set_name(u"verification");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"Submit");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(7U, form_structure->field_count());
  ASSERT_EQ(5U, form_structure->autofill_count());

  // Credit card name.
  EXPECT_EQ(CREDIT_CARD_NAME_FULL, form_structure->field(0)->heuristic_type());
  // Credit card type.  This is an unknown type but related to the credit card.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(1)->heuristic_type());
  // Credit card number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  // Credit card expiration month.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Credit card expiration year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
  // CVV.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(5)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
}

TEST_F(FormStructureTestImpl, ThreeAddressLines) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Address Line1");
  field.set_name(u"Address");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address Line2");
  field.set_name(u"Address");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address Line3");
  field.set_name(u"Address");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"City");
  field.set_name(u"city");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(ADDRESS_HOME_LINE3, form_structure->field(2)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(3)->heuristic_type());
}

// Numbered address lines after line two are ignored.
TEST_F(FormStructureTestImpl, SurplusAddressLinesIgnored) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Address Line1");
  field.set_name(u"shipping.address.addressLine1");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address Line2");
  field.set_name(u"shipping.address.addressLine2");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address Line3");
  field.set_name(u"billing.address.addressLine3");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address Line4");
  field.set_name(u"billing.address.addressLine4");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(ADDRESS_HOME_LINE3, form_structure->field(2)->heuristic_type());
  // Address Line 4 (ignored).
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(3)->heuristic_type());
}

// This example comes from expedia.com where they used to use a "Suite" label
// to indicate a suite or apartment number (the form has changed since this
// test was written). We interpret this as address line 2. And the following
// "Street address second line" we interpret as address line 3.
// See http://crbug.com/48197 for details.
TEST_F(FormStructureTestImpl, ThreeAddressLinesExpedia) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Street:");
  field.set_name(u"FOPIH_RgWebCC_0_IHAddress_ads1");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Suite or Apt:");
  field.set_name(u"FOPIH_RgWebCC_0_IHAddress_adap");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Street address second line");
  field.set_name(u"FOPIH_RgWebCC_0_IHAddress_ads2");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"City:");
  field.set_name(u"FOPIH_RgWebCC_0_IHAddress_adct");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(4U, form_structure->field_count());
  EXPECT_EQ(4U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Suite / Apt.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(ADDRESS_HOME_LINE3, form_structure->field(2)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(3)->heuristic_type());
}

// This example comes from ebay.com where the word "suite" appears in the label
// and the name "address2" clearly indicates that this is the address line 2.
// See http://crbug.com/48197 for details.
TEST_F(FormStructureTestImpl, TwoAddressLinesEbay) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Address Line1");
  field.set_name(u"address1");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Floor number, suite number, etc");
  field.set_name(u"address2");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"City:");
  field.set_name(u"city");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(2)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsStateWithProvince) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Address Line1");
  field.set_name(u"Address");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address Line2");
  field.set_name(u"Address");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"State/Province/Region");
  field.set_name(u"State");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // State.
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure->field(2)->heuristic_type());
}

// This example comes from lego.com's checkout page.
TEST_F(FormStructureTestImpl, HeuristicsWithBilling) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"First Name*:");
  field.set_name(u"editBillingAddress$firstNameBox");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Last Name*:");
  field.set_name(u"editBillingAddress$lastNameBox");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Company Name:");
  field.set_name(u"editBillingAddress$companyBox");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address*:");
  field.set_name(u"editBillingAddress$addressLine1Box");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Apt/Suite :");
  field.set_name(u"editBillingAddress$addressLine2Box");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"City*:");
  field.set_name(u"editBillingAddress$cityBox");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"State/Province*:");
  field.set_name(u"editBillingAddress$stateDropDown");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Country*:");
  field.set_name(u"editBillingAddress$countryDropDown");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Postal Code*:");
  field.set_name(u"editBillingAddress$zipCodeBox");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Phone*:");
  field.set_name(u"editBillingAddress$phoneBox");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Email Address*:");
  field.set_name(u"email$emailBox");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(11U, form_structure->field_count());
  ASSERT_EQ(11U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(COMPANY_NAME, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(4)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(5)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure->field(6)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, form_structure->field(7)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(8)->heuristic_type());
  EXPECT_EQ(PHONE_HOME_CITY_AND_NUMBER,
            form_structure->field(9)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(10)->heuristic_type());
}

TEST_F(FormStructureTestImpl, ThreePartPhoneNumber) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Phone:");
  field.set_name(u"dayphone1");
  field.set_max_length(0);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"-");
  field.set_name(u"dayphone2");
  field.set_max_length(3);  // Size of prefix is 3.
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"-");
  field.set_name(u"dayphone3");
  field.set_max_length(4);  // Size of suffix is 4.  If unlimited size is
                            // passed, phone will be parsed as
                            // <country code> - <area code> - <phone>.
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"ext.:");
  field.set_name(u"dayphone4");
  field.set_max_length(0);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

  EXPECT_EQ(PHONE_HOME_CITY_CODE, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(PHONE_HOME_NUMBER_PREFIX,
            form_structure->field(1)->heuristic_type());
  EXPECT_EQ(PHONE_HOME_NUMBER_SUFFIX,
            form_structure->field(2)->heuristic_type());
  EXPECT_EQ(PHONE_HOME_EXTENSION, form_structure->field(3)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsInfernoCC) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Name on Card");
  field.set_name(u"name_on_card");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Address");
  field.set_name(u"billing_address");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Card Number");
  field.set_name(u"card_number");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Expiration Date");
  field.set_name(u"expiration_month");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Expiration Year");
  field.set_name(u"expiration_year");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());
  EXPECT_EQ(5U, form_structure->autofill_count());

  // Name on Card.
  EXPECT_EQ(CREDIT_CARD_NAME_FULL, form_structure->field(0)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(1)->heuristic_type());
  // Card Number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  // Expiration Date.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Expiration Year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
}

// Tests that the heuristics detect split credit card names if they appear in
// the middle of the form.
TEST_F(FormStructureTestImpl, HeuristicsInferCCNames_NamesNotFirst) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Card number");
  field.set_name(u"ccnumber");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"First name");
  field.set_name(u"first_name");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Last name");
  field.set_name(u"last_name");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Expiration date");
  field.set_name(u"ccexpiresmonth");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"ccexpiresyear");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"cvc number");
  field.set_name(u"csc");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(6U, form_structure->field_count());
  ASSERT_EQ(6U, form_structure->autofill_count());

  // Card Number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(0)->heuristic_type());
  // First name.
  EXPECT_EQ(CREDIT_CARD_NAME_FIRST, form_structure->field(1)->heuristic_type());
  // Last name.
  EXPECT_EQ(CREDIT_CARD_NAME_LAST, form_structure->field(2)->heuristic_type());
  // Expiration Date.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Expiration Year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
  // CVC code.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(5)->heuristic_type());
}

// Tests that the heuristics detect split credit card names if they appear at
// the beginning of the form. The first name has to contains some credit card
// keyword.
TEST_F(FormStructureTestImpl, HeuristicsInferCCNames_NamesFirst) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Cardholder Name");
  field.set_name(u"cc_first_name");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Last name");
  field.set_name(u"last_name");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Card number");
  field.set_name(u"ccnumber");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Expiration date");
  field.set_name(u"ccexpiresmonth");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"ccexpiresyear");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"cvc number");
  field.set_name(u"csc");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                          nullptr);
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(6U, form_structure->field_count());
  ASSERT_EQ(6U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(CREDIT_CARD_NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(CREDIT_CARD_NAME_LAST, form_structure->field(1)->heuristic_type());
  // Card Number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  // Expiration Date.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Expiration Year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
  // CVC code.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(5)->heuristic_type());
}

TEST_F(FormStructureTestImpl, ButtonTitleType_Match) {
  // Statically assert that the mojom::ButtonTitleType enum matches the
  // corresponding entries in the proto - ButtonTitleType enum.
  static_assert(
      ButtonTitleType::NONE == static_cast<int>(mojom::ButtonTitleType::NONE),
      "NONE enumerator does not match!");

  static_assert(
      ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE ==
          static_cast<int>(mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE),
      "BUTTON_ELEMENT_SUBMIT_TYPE enumerator does not match!");

  static_assert(
      ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE ==
          static_cast<int>(mojom::ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE),
      "BUTTON_ELEMENT_BUTTON_TYPE enumerator does not match!");

  static_assert(
      ButtonTitleType::INPUT_ELEMENT_SUBMIT_TYPE ==
          static_cast<int>(mojom::ButtonTitleType::INPUT_ELEMENT_SUBMIT_TYPE),
      "INPUT_ELEMENT_SUBMIT_TYPE enumerator does not match!");

  static_assert(
      ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE ==
          static_cast<int>(mojom::ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE),
      "INPUT_ELEMENT_BUTTON_TYPE enumerator does not match!");

  static_assert(ButtonTitleType::HYPERLINK ==
                    static_cast<int>(mojom::ButtonTitleType::HYPERLINK),
                "HYPERLINK enumerator does not match!");

  static_assert(
      ButtonTitleType::DIV == static_cast<int>(mojom::ButtonTitleType::DIV),
      "DIV enumerator does not match!");

  static_assert(
      ButtonTitleType::SPAN == static_cast<int>(mojom::ButtonTitleType::SPAN),
      "SPAN enumerator does not match!");
}

TEST_F(FormStructureTestImpl, CheckFormSignature) {
  // Check that form signature is created correctly.
  std::unique_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"email");
  field.set_name(u"email");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"First Name");
  field.set_name(u"first");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  // Checkable fields shouldn't affect the signature.
  field.set_label(u"Select");
  field.set_name(u"Select");
  field.set_form_control_type(FormControlType::kInputCheckbox);
  field.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);

  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(std::string("://&&email&first")),
            form_structure->FormSignatureAsStr());

  form.set_url(GURL(std::string("http://www.facebook.com")));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("http://www.facebook.com&&email&first")),
            form_structure->FormSignatureAsStr());

  form.set_action(GURL(std::string("https://login.facebook.com/path")));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("https://login.facebook.com&&email&first")),
            form_structure->FormSignatureAsStr());

  form.set_name(u"login_form");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(std::string(
                "https://login.facebook.com&login_form&email&first")),
            form_structure->FormSignatureAsStr());

  // Checks how digits are removed from field names.
  field.set_check_status(FormFieldData::CheckStatus::kNotCheckable);
  field.set_label(u"Random Field label");
  field.set_name(u"random1234");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Random Field label2");
  field.set_name(u"random12345");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Random Field label3");
  field.set_name(u"1ran12dom12345678");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Random Field label3");
  field.set_name(u"12345ran123456dom123");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("https://login.facebook.com&login_form&email&first&"
                            "random1234&random&1ran12dom&random123")),
            form_structure->FormSignatureAsStr());
}

TEST_F(FormStructureTestImpl, CheckAlternativeFormSignatureLarge) {
  FormData large_form;
  large_form.set_url(GURL("http://foo.com/login?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(large_form).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(large_form).Append(field2);

  FormFieldData field3;
  field3.set_form_control_type(FormControlType::kInputEmail);
  test_api(large_form).Append(field3);

  FormFieldData field4;
  field4.set_form_control_type(FormControlType::kInputTelephone);
  test_api(large_form).Append(field4);

  // Alternative form signature string of a form with more than two fields
  // should only concatenate scheme, host, and field types.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text&email&tel"),
            std::make_unique<FormStructure>(large_form)
                ->alternative_form_signature()
                .value());
}

TEST_F(FormStructureTestImpl, CheckAlternativeFormSignatureSmallPath) {
  FormData small_form_path;
  small_form_path.set_url(GURL("http://foo.com/login?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_path).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_path).Append(field2);

  // Alternative form signature string of a form with 2 fields or less should
  // concatenate scheme, host, field types, and path if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text/login"),
            std::make_unique<FormStructure>(small_form_path)
                ->alternative_form_signature()
                .value());
}

TEST_F(FormStructureTestImpl, CheckAlternativeFormSignatureSmallRef) {
  FormData small_form_ref;
  small_form_ref.set_url(GURL("http://foo.com?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_ref).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_ref).Append(field2);

  // Alternative form signature string of a form with 2 fields or less and
  // without a path should concatenate scheme, host, field types, and reference
  // if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text#ref"),
            std::make_unique<FormStructure>(small_form_ref)
                ->alternative_form_signature()
                .value());
}

TEST_F(FormStructureTestImpl, CheckAlternativeFormSignatureSmallQuery) {
  FormData small_form_query;
  small_form_query.set_url(GURL("http://foo.com?q=a"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_query).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_query).Append(field2);

  // Alternative form signature string of a form with 2 fields or less and
  // without a path or reference should concatenate scheme, host, field types,
  // and query if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text?q=a"),
            std::make_unique<FormStructure>(small_form_query)
                ->alternative_form_signature()
                .value());
}

TEST_F(FormStructureTestImpl, ToFormData) {
  FormData form;
  form.set_name(u"the-name");
  form.set_url(GURL("http://cool.com"));
  form.set_action(form.url().Resolve("/login"));
  form.set_child_frames({FrameTokenWithPredecessor()});

  FormFieldData field;
  field.set_label(u"username");
  field.set_name(u"username");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"password");
  field.set_name(u"password");
  field.set_form_control_type(FormControlType::kInputPassword);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(std::u16string());
  field.set_name(u"Submit");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_TRUE(FormData::DeepEqual(form, FormStructure(form).ToFormData()));
}

// Tests that an Autofill upload for password form with 1 field should not be
// uploaded.
TEST_F(FormStructureTestImpl, OneFieldPasswordFormShouldNotBeUpload) {
  FormData form;
  FormFieldData field;
  field.set_name(u"Password");
  field.set_form_control_type(FormControlType::kInputPassword);
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  EXPECT_FALSE(FormStructure(form).ShouldBeUploaded());
}


// Tests if a new logical form is started with the second appearance of a field
// of type |FieldTypeGroup::kName|.
TEST_F(FormStructureTestImpl, NoAutocompleteSectionNames) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_max_length(10000);

  field.set_label(u"Full Name");
  field.set_name(u"fullName");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Country");
  field.set_name(u"country");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Phone");
  field.set_name(u"phone");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Full Name");
  field.set_name(u"fullName");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Country");
  field.set_name(u"country");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Phone");
  field.set_name(u"phone");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes({NAME_FULL, ADDRESS_HOME_COUNTRY, PHONE_HOME_NUMBER,
                      NAME_FULL, ADDRESS_HOME_COUNTRY, PHONE_HOME_NUMBER});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).AssignSections();

  // Assert the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());
  EXPECT_EQ("fullName_0_11", form_structure.field(0)->section().ToString());
  EXPECT_EQ("fullName_0_11", form_structure.field(1)->section().ToString());
  EXPECT_EQ("fullName_0_11", form_structure.field(2)->section().ToString());
  EXPECT_EQ("fullName_0_14", form_structure.field(3)->section().ToString());
  EXPECT_EQ("fullName_0_14", form_structure.field(4)->section().ToString());
  EXPECT_EQ("fullName_0_14", form_structure.field(5)->section().ToString());
}

// Tests that adjacent name field types are not split into different sections.
TEST_F(FormStructureTestImpl, NoSplitAdjacentNameFieldType) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Phonetic First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Phonetic Last Name", "lastname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Country", "country", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText)});
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes({NAME_FIRST, NAME_LAST, NAME_FIRST, NAME_LAST,
                      ADDRESS_HOME_COUNTRY, NAME_FIRST});

  test_api(form_structure).AssignSections();

  // Assert the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());

  EXPECT_EQ(form_structure.field(0)->section(),
            form_structure.field(1)->section());
  EXPECT_EQ(form_structure.field(0)->section(),
            form_structure.field(2)->section());
  EXPECT_EQ(form_structure.field(0)->section(),
            form_structure.field(3)->section());
  EXPECT_EQ(form_structure.field(0)->section(),
            form_structure.field(4)->section());
  // The non-adjacent name field should be split into a different section.
  EXPECT_NE(form_structure.field(0)->section(),
            form_structure.field(5)->section());
}

TEST_F(FormStructureTestImpl, FindFieldsEligibleForManualFilling) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_max_length(10000);

  field.set_label(u"Full Name");
  field.set_name(u"fullName");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);
  FieldGlobalId full_name_id = field.global_id();

  field.set_label(u"Country");
  field.set_name(u"country");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  field.set_label(u"Unknown");
  field.set_name(u"unknown");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);
  FieldGlobalId unknown_id = field.global_id();

  FormStructure form_structure(form);

  test_api(form_structure)
      .SetFieldTypes(
          {CREDIT_CARD_NAME_FULL, ADDRESS_HOME_COUNTRY, UNKNOWN_TYPE});

  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).AssignSections();
  std::vector<FieldGlobalId> expected_result;
  // Only credit card related and unknown fields are eligible for manual
  // filling.
  expected_result.push_back(full_name_id);
  expected_result.push_back(unknown_id);

  EXPECT_EQ(expected_result,
            FormStructure::FindFieldsEligibleForManualFilling(forms));
}

TEST_F(FormStructureTestImpl, DetermineRanks) {
  FormData form;
  form.set_url(GURL("http://foo.com"));

  auto add_field = [&form](const std::u16string& name,
                           LocalFrameToken frame_token,
                           FormRendererId host_form_id) {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputText);
    field.set_name(name);
    field.set_renderer_id(test::MakeFieldRendererId());
    field.set_host_frame(frame_token);
    field.set_host_form_id(host_form_id);
    test_api(form).Append(field);
  };

  LocalFrameToken frame_1(base::UnguessableToken::Create());
  LocalFrameToken frame_2(base::UnguessableToken::Create());
  add_field(u"A", frame_1, FormRendererId(1));  // First form
  add_field(u"B", frame_1, FormRendererId(1));
  add_field(u"A", frame_1, FormRendererId(1));
  add_field(u"A", frame_2, FormRendererId(2));  // Second form
  add_field(u"B", frame_2, FormRendererId(2));
  add_field(u"A", frame_2, FormRendererId(3));  // Third form

  FormStructure form_structure(form);

  auto extract = [&form_structure](size_t (AutofillField::*fun)() const) {
    std::vector<size_t> result;
    for (const auto& field : form_structure.fields()) {
      result.push_back(std::invoke(fun, *field));
    }
    return result;
  };

  EXPECT_THAT(extract(&AutofillField::rank), ElementsAre(0, 1, 2, 3, 4, 5));
  EXPECT_THAT(extract(&AutofillField::rank_in_signature_group),
              ElementsAre(0, 0, 1, 2, 1, 3));
  EXPECT_THAT(extract(&AutofillField::rank_in_host_form),
              ElementsAre(0, 1, 2, 0, 1, 0));
  EXPECT_THAT(extract(&AutofillField::rank_in_host_form_signature_group),
              ElementsAre(0, 0, 1, 0, 0, 0));
}

// Tests that forms that are completely annotated with ac=unrecognized are not
// classified as address forms.
TEST_F(FormStructureTestImpl, GetFormTypes_AutocompleteUnrecognized) {
  FormData form = test::CreateTestAddressFormData();
  for (FormFieldData& field : test_api(form).fields()) {
    field.set_parsed_autocomplete(
        AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized});
  }
  FormStructure form_structure(form);
  EXPECT_THAT(form_structure.GetFormTypes(),
              UnorderedElementsAre(FormType::kUnknownFormType));
}

// By default, the single field email heuristics are off. Although applying
// heuristics in this case appears to have been intended behavior, the rollout
// must be managed with care. This test is intended to ensure the default
// experience does not change unintentionally.
TEST_F(FormStructureTestImpl, SingleFieldEmailHeuristicsDefaultBehavior) {
  FormData form = test::GetFormData({.fields = {{.role = EMAIL_ADDRESS}}});

  // The form has too few fields; it should not run heuristics, falling back to
  // the single field parsing.
  EXPECT_FALSE(FormShouldRunHeuristics(form));
  EXPECT_TRUE(FormShouldRunHeuristicsForSingleFieldForms(form));

  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(1U, form_structure.field_count());
    ASSERT_EQ(0U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_FALSE(form_structure.IsAutofillable());
  }
}

// When the single field email heuristics feature is enabled, a single field
// email form should be parsed accordingly.
TEST_F(FormStructureTestImpl, SingleFieldEmailHeuristicsEnabled) {
  base::test::ScopedFeatureList enabled{
      features::kAutofillEnableEmailHeuristicOnlyAddressForms};

  FormData form = test::GetFormData({.fields = {{.role = EMAIL_ADDRESS}}});

  // The form has too few fields; it should not run heuristics, falling back to
  // the single field parsing.
  EXPECT_FALSE(FormShouldRunHeuristics(form));
  EXPECT_TRUE(FormShouldRunHeuristicsForSingleFieldForms(form));

  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(1U, form_structure.field_count());
    // However, because the email field is in a form and matches the heuristics,
    // it should be autofillable when the feature is enabled.
    ASSERT_EQ(1U, form_structure.autofill_count());
    EXPECT_EQ(EMAIL_ADDRESS, form_structure.field(0)->heuristic_type());
    EXPECT_TRUE(form_structure.IsAutofillable());
  }
}

// Verifies that with kAutofillEnableEmailHeuristicAutocompleteEmail enabled,
// only fields with autocomplete=email are parsed as email fields.
TEST_F(FormStructureTestImpl,
       SingleFieldEmailHeuristicsEnabledAutocompleteEmail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillEnableEmailHeuristicOnlyAddressForms,
      base::FieldTrialParams{
          {features::kAutofillEnableEmailHeuristicAutocompleteEmail.name,
           "true"}});

  FormData form = test::GetFormData(
      {.fields = {{.role = EMAIL_ADDRESS, .autocomplete_attribute = "off"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"}}});

  // The form has too few fields; it should not run heuristics, falling back to
  // the single field parsing.
  EXPECT_FALSE(FormShouldRunHeuristics(form));
  EXPECT_TRUE(FormShouldRunHeuristicsForSingleFieldForms(form));

  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(2U, form_structure.field_count());
    ASSERT_EQ(1U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(EMAIL_ADDRESS, form_structure.field(1)->heuristic_type());
    EXPECT_TRUE(form_structure.IsAutofillable());
  }
}

// When the single field email heuristics feature is enabled, email fields are
// not parsed if these are outside of form tags.
TEST_F(FormStructureTestImpl,
       SingleFieldEmailHeuristicsNotSupportedOutsideFormTag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillEnableEmailHeuristicOnlyAddressForms},
      {features::kAutofillEnableEmailHeuristicOutsideForms});

  FormData form = test::GetFormData({.fields = {{.role = EMAIL_ADDRESS}}});
  // Set the form to simulate a field outside a <form> tag.
  form.set_renderer_id(FormRendererId());

  // The form has too few fields; it should not run heuristics, falling back to
  // the single field parsing.
  EXPECT_FALSE(FormShouldRunHeuristics(form));
  EXPECT_TRUE(FormShouldRunHeuristicsForSingleFieldForms(form));
  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(1U, form_structure.field_count());
    ASSERT_EQ(0U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_FALSE(form_structure.IsAutofillable());
  }
}

// When the single field email heuristics feature is enabled, a single field
// email form should be parsed accordingly. Support for email fields outside of
// form tags is also supported when `kAutofillEnableEmailHeuristicOutsideForms`
// is enabled.
TEST_F(FormStructureTestImpl,
       SingleFieldEmailHeuristicsSupportedOutsideFormTag) {
  base::test::ScopedFeatureList enabled;
  enabled.InitWithFeatures(
      {features::kAutofillEnableEmailHeuristicOnlyAddressForms,
       features::kAutofillEnableEmailHeuristicOutsideForms},
      {});

  FormData form = test::GetFormData({.fields = {{.role = EMAIL_ADDRESS}}});
  // Set the form to simulate a field outside a <form> tag.
  form.set_renderer_id(FormRendererId());

  // The form has too few fields; it should not run heuristics, falling back to
  // the single field parsing.
  EXPECT_FALSE(FormShouldRunHeuristics(form));
  EXPECT_TRUE(FormShouldRunHeuristicsForSingleFieldForms(form));
  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                           nullptr);
    ASSERT_EQ(1U, form_structure.field_count());
    // However, because the email field is in a form and matches the heuristics,
    // it should be autofillable when the feature is enabled.
    ASSERT_EQ(1U, form_structure.autofill_count());
    EXPECT_EQ(EMAIL_ADDRESS, form_structure.field(0)->heuristic_type());
    EXPECT_TRUE(form_structure.IsAutofillable());
  }
}

}  // namespace
}  // namespace autofill
