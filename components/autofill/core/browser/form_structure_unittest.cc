// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
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
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
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

constexpr uint64_t kFieldMaxLength = 10000;

constexpr DenseSet<PatternSource> kAllPatternSources {
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  PatternSource::kLegacy
#else
    PatternSource::kDefault, PatternSource::kExperimental,
    PatternSource::kNextGen
#endif
};

}  // namespace

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

class FormStructureTest_ForPatternSource
    : public FormStructureTestImpl,
      public testing::WithParamInterface<PatternSource> {
 public:
  FormStructureTest_ForPatternSource() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
            features::kAutofillParsingPatternProvider,
            {{"prediction_source", pattern_source_as_string()}})},
        {});
  }

  PatternSource pattern_source() const { return GetParam(); }

  std::string pattern_source_as_string() const {
    switch (pattern_source()) {
      case PatternSource::kLegacy:
        return "legacy";
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
      case PatternSource::kDefault:
        return "default";
      case PatternSource::kExperimental:
        return "experimental";
      case PatternSource::kNextGen:
        return "nextgen";
#endif
    }
  }

  DenseSet<PatternSource> other_pattern_sources() const {
    DenseSet<PatternSource> patterns = kAllPatternSources;
    patterns.erase(pattern_source());
    return patterns;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(FormStructureTest,
                         FormStructureTest_ForPatternSource,
                         ::testing::ValuesIn(kAllPatternSources));

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
  form.url = GURL("http://www.foo.com/");
  FormStructure form_structure(form);

  EXPECT_EQ(form.url, form_structure.source_url());
}

TEST_F(FormStructureTestImpl, FullSourceURLWithHashAndParam) {
  FormData form;
  form.full_url = GURL("https://www.foo.com/?login=asdf#hash");
  FormStructure form_structure(form);

  EXPECT_EQ(form.full_url, form_structure.full_source_url());
}

TEST_F(FormStructureTestImpl, IsAutofillable) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  FormFieldData field;

  // Start with a username field. It should be picked up by the password but
  // not by autofill.
  field.label = u"username";
  field.name = u"username";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // With min required fields enabled.
  EXPECT_FALSE(FormIsAutofillable(form));

  // Add a password field. The form should be picked up by the password but
  // not by autofill.
  field.label = u"password";
  field.name = u"password";
  field.form_control_type = FormControlType::kInputPassword;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.label = u"Full Name";
  field.name = u"fullname";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.label = u"Address Line 1";
  field.name = u"address1";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // We now have three auto-fillable fields. It's always autofillable.
  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = FormControlType::kInputEmail;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_TRUE(FormIsAutofillable(form));

  // The target cannot include http(s)://*/search...
  form.action = GURL("http://google.com/search?q=hello");

  EXPECT_FALSE(FormIsAutofillable(form));

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");

  EXPECT_TRUE(FormIsAutofillable(form));
}

class FormStructureTestImpl_ShouldBeParsed_Test : public FormStructureTestImpl {
 public:
  FormStructureTestImpl_ShouldBeParsed_Test() {
    form_.url = GURL("http://www.foo.com/");
    form_structure_ = std::make_unique<FormStructure>(form_);
  }

  ~FormStructureTestImpl_ShouldBeParsed_Test() override = default;

  void SetAction(GURL action) {
    form_.action = action;
    form_structure_ = nullptr;
  }

  void AddField(FormFieldData field) {
    field.renderer_id = test::MakeFieldRendererId();
    form_.fields.push_back(std::move(field));
    form_structure_ = nullptr;
  }

  void AddTextField() {
    FormFieldData field;
    field.form_control_type = FormControlType::kInputText;
    AddField(field);
  }

  FormStructure* form_structure() {
    if (!form_structure_)
      form_structure_ = std::make_unique<FormStructure>(form_);
    return form_structure_.get();
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
    field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
    field.form_control_type = FormControlType::kInputRadio;
    AddField(field);
  }
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));

  // Add a second checkable field.
  {
    FormFieldData field;
    field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
    field.form_control_type = FormControlType::kInputCheckbox;
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
    field.form_control_type = FormControlType::kSelectOne;
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

TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, FalseIfOnlySelectListField) {
  {
    FormFieldData field;
    field.form_control_type = FormControlType::kSelectList;
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
    field.form_control_type = FormControlType::kInputPassword;
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
    field.form_control_type = FormControlType::kInputPassword;
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
    field.parsed_autocomplete = AutocompleteParsingResult{
        .section = "my-billing-section", .field_type = HtmlFieldType::kName};
    field.form_control_type = FormControlType::kInputText;
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
  form.fields = {
      CreateTestFormField("Name", "name", "", FormControlType::kInputText,
                          "name"),
      CreateTestFormField("Email", "email", "", FormControlType::kInputText,
                          "email"),
      CreateTestFormField("Address", "address", "", FormControlType::kInputText,
                          "address-line1")};

  // Baseline, HTTP should work.
  form.url = GURL("http://wwww.foo.com/myform");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Baseline, HTTPS should work.
  form.url = GURL("https://wwww.foo.com/myform");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Chrome internal urls shouldn't be parsed.
  form.url = GURL("chrome://settings");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // FTP urls shouldn't be parsed.
  form.url = GURL("ftp://ftp.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // Blob urls shouldn't be parsed.
  form.url = GURL("blob://blob.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // About urls shouldn't be parsed.
  form.url = GURL("about://about.foo.com/form.html");
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
  form.url = GURL("http://www.foo.com/");
  form.fields = {CreateTestFormField("Name", "name", "",
                                     FormControlType::kInputText, "name"),
                 CreateTestFormField("Address", "Address", "",
                                     FormControlType::kSelectOne, "")};
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
  FieldType expected_phone_number =
      base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber)
          ? PHONE_HOME_CITY_AND_NUMBER
          : PHONE_HOME_WHOLE_NUMBER;
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
             NAME_FIRST, NAME_LAST, EMAIL_ADDRESS, expected_phone_number,
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
               .parsed_autocomplete = ParseAutocompleteAttribute("email")},
              {.label = u"",
               .name = u"field4",
               .autocomplete_attribute = "upi-vpa",
               .parsed_autocomplete = ParseAutocompleteAttribute("upi-vpa")}}},
        {
            .determine_heuristic_type = true,
            .is_autofillable = true,
            .has_author_specified_types = true,
            .has_author_specified_upi_vpa_hint = true,
            .field_count = 4,
            .autofill_count = 3,
        },
        {.expected_html_type = {HtmlFieldType::kGivenName,
                                HtmlFieldType::kFamilyName,
                                HtmlFieldType::kEmail,
                                HtmlFieldType::kUnrecognized},
         .expected_heuristic_type = {UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE,
                                     UNKNOWN_TYPE}}}});
}

// All fields share a common prefix which could confuse the heuristics. Test
// that the common prefix is stripped out before running heuristics.
TEST_F(FormStructureTestImpl, StripCommonNamePrefix) {
  FieldType expected_phone_number =
      base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber)
          ? PHONE_HOME_CITY_AND_NUMBER
          : PHONE_HOME_WHOLE_NUMBER;
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
                                     expected_phone_number, UNKNOWN_TYPE}}}});
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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = u"firstname";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");
  // Set a valid autocomplete attribute to the first field.
  form.fields = {CreateTestFormField("First Name", "firstname", "",
                                     FormControlType::kInputText, "given-name"),
                 CreateTestFormField("Last Name", "lastname", "",
                                     FormControlType::kInputText, "")};
  EXPECT_FALSE(FormShouldRunHeuristics(form));
  EXPECT_TRUE(FormShouldBeQueried(form));

  // As a side effect of parsing small forms (if any of the heuristics, query,
  // or upload minimums are disabled, we'll autofill fields with an
  // autocomplete attribute, even if its the only field in the form.
  {
    FormData form_copy = form;
    form_copy.fields.pop_back();
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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Promo Code";
  field.name = u"promocode";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");
  form.fields = {CreateTestFormField("First Name", "firstname", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Last Name", "lastname", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Email", "email", "",
                                     FormControlType::kInputText, "username"),
                 CreateTestFormField("Password", "Password", "",
                                     FormControlType::kInputPassword)};
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                         nullptr);
  EXPECT_TRUE(form_structure.has_password_field());
  EXPECT_TRUE(form_structure.ShouldBeQueried());
  EXPECT_TRUE(form_structure.ShouldBeUploaded());
}

// Verify that we can correctly process sections listed in the |autocomplete|
// attribute.
TEST_F(FormStructureTestImpl, HeuristicsAutocompleteAttributeWithSections) {
  // This test tests whether credit card fields are implicitly in one, separate
  // credit card section, independent of whether they have a valid autocomplete
  // attribute section. With the new sectioning, credit card fields with a valid
  // autocomplete attribute section S are in section S.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillUseParameterizedSectioning);

  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.fields = {
      // Some fields will have no section specified.  These fall into the
      // default section.
      CreateTestFormField("", "", "", FormControlType::kInputText, "email"),
      // We allow arbitrary section names.
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "section-foo email"),
      // "shipping" and "billing" are special section tokens that don't require
      // the "section-" prefix.
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "shipping email"),
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "billing email"),
      // "shipping" and "billing" can be combined with other section names.
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "section-foo shipping email"),
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "section-foo billing email"),
      // We don't do anything clever to try to coalesce sections; it's up to
      // site authors to avoid typos.
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "section--foo email"),
      // "shipping email" and "section--shipping" email should be parsed as
      // different sections.  This is only an interesting test due to how we
      // implement implicit section names from attributes like "shipping email";
      // see the implementation for more details.
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "section--shipping email"),
      // Credit card fields are implicitly in one, separate credit card section.
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "section-foo cc-number")};
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                         nullptr);
  EXPECT_TRUE(form_structure.IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(9U, form_structure.field_count());
  EXPECT_EQ(9U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to different
  // sections.
  std::set<Section> section_names;
  for (size_t i = 0; i < 9; ++i) {
    section_names.insert(form_structure.field(i)->section);
  }
  EXPECT_EQ(9U, section_names.size());
}

// Verify that we can correctly process a degenerate section listed in the
// |autocomplete| attribute.
TEST_F(FormStructureTestImpl,
       HeuristicsAutocompleteAttributeWithSectionsDegenerate) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.fields = {
      // Some fields will have no section specified.  These fall into the
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
                          "garbage billing email")};
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
    section_names.insert(form_structure.field(i)->section);
  }
  EXPECT_EQ(1U, section_names.size());
}

// Verify that we can correctly process repeated sections listed in the
// |autocomplete| attribute.
TEST_F(FormStructureTestImpl,
       HeuristicsAutocompleteAttributeWithSectionsRepeated) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.fields = {CreateTestFormField("", "", "", FormControlType::kInputText,
                                     "section-foo email"),
                 CreateTestFormField("", "", "", FormControlType::kInputText,
                                     "section-foo address-line1")};
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
    section_names.insert(form_structure.field(i)->section);
  }
  EXPECT_EQ(1U, section_names.size());
}

// Verify that we do not override the author-specified sections from a form with
// local heuristics.
TEST_F(FormStructureTestImpl,
       HeuristicsDontOverrideAutocompleteAttributeSections) {
  // With the new sectioning, fields with a valid autocomplete attribute section
  // S are in section S. All other <input> fields that are focusable are
  // partitioned into intervals, each of which is a section.
  // This is different compared to the old behavior which assigns fields without
  // an autocomplete attribute section to the empty, "-default" section if there
  // is a field with a valid autocomplete attribute section in the form.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillUseParameterizedSectioning);

  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.fields = {
      CreateTestFormField("", "one", "", FormControlType::kInputText,
                          "address-line1"),
      CreateTestFormField("", "", "", FormControlType::kInputText,
                          "section-foo email"),
      CreateTestFormField("", "", "", FormControlType::kInputText, "name"),
      CreateTestFormField("", "two", "", FormControlType::kInputText,
                          "address-line1")};
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                         nullptr);

  // Expect the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());
  EXPECT_EQ(4U, form_structure.autofill_count());

  // Normally, the two separate address fields would cause us to detect two
  // separate sections; but because there is an author-specified section in this
  // form, we do not apply these usual heuristics.
  EXPECT_EQ(u"one", form_structure.field(0)->name);
  EXPECT_EQ(u"two", form_structure.field(3)->name);
  EXPECT_EQ(form_structure.field(0)->section, form_structure.field(3)->section);
}

TEST_F(FormStructureTestImpl, HeuristicsSample8) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Your First Name:";
  field.name = u"bill.first";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Your Last Name:";
  field.name = u"bill.last";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Street Address Line 1:";
  field.name = u"bill.street1";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Street Address Line 2:";
  field.name = u"bill.street2";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City";
  field.name = u"bill.city";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"State (U.S.):";
  field.name = u"bill.state";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Zip/Postal Code:";
  field.name = u"BillTo.PostalCode";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country:";
  field.name = u"bill.country";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone Number:";
  field.name = u"BillTo.Phone";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  FieldType expected_phone_number =
      base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber)
          ? PHONE_HOME_CITY_AND_NUMBER
          : PHONE_HOME_WHOLE_NUMBER;
  EXPECT_EQ(expected_phone_number, form_structure->field(8)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(9)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsSample6) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"E-mail address";
  field.name = u"email";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Full name";
  field.name = u"name";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Company";
  field.name = u"company";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = u"address";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City";
  field.name = u"city";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Zip Code";
  field.name = u"Home.PostalCode";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.value = u"continue";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = std::u16string();
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = std::u16string();
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = std::u16string();
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = std::u16string();
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = std::u16string();
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = std::u16string();
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Zip code";
  field.name = std::u16string();
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  FieldType expected_phone_number =
      base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber)
          ? PHONE_HOME_CITY_AND_NUMBER
          : PHONE_HOME_WHOLE_NUMBER;
  EXPECT_EQ(expected_phone_number, form_structure->field(3)->heuristic_type());
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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Name on Card";
  field.name = u"name_on_card";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card Number";
  field.name = u"card_number";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Month";
  field.name = u"ccmonth";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Year";
  field.name = u"ccyear";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Verification";
  field.name = u"verification";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Name on Card";
  field.name = u"name_on_card";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // This is not a field we know how to process.  But we should skip over it
  // and process the other fields in the card block.
  field.label = u"Card image";
  field.name = u"card_image";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card Number";
  field.name = u"card_number";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Month";
  field.name = u"ccmonth";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Year";
  field.name = u"ccyear";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Verification";
  field.name = u"verification";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Address Line1";
  field.name = u"Address";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line2";
  field.name = u"Address";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line3";
  field.name = u"Address";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City";
  field.name = u"city";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Address Line1";
  field.name = u"shipping.address.addressLine1";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line2";
  field.name = u"shipping.address.addressLine2";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line3";
  field.name = u"billing.address.addressLine3";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line4";
  field.name = u"billing.address.addressLine4";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Street:";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_ads1";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Suite or Apt:";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_adap";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Street address second line";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_ads2";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City:";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_adct";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Address Line1";
  field.name = u"address1";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Floor number, suite number, etc";
  field.name = u"address2";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City:";
  field.name = u"city";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Address Line1";
  field.name = u"Address";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line2";
  field.name = u"Address";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"State/Province/Region";
  field.name = u"State";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name*:";
  field.name = u"editBillingAddress$firstNameBox";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name*:";
  field.name = u"editBillingAddress$lastNameBox";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Company Name:";
  field.name = u"editBillingAddress$companyBox";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address*:";
  field.name = u"editBillingAddress$addressLine1Box";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Apt/Suite :";
  field.name = u"editBillingAddress$addressLine2Box";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City*:";
  field.name = u"editBillingAddress$cityBox";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"State/Province*:";
  field.name = u"editBillingAddress$stateDropDown";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country*:";
  field.name = u"editBillingAddress$countryDropDown";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Postal Code*:";
  field.name = u"editBillingAddress$zipCodeBox";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone*:";
  field.name = u"editBillingAddress$phoneBox";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email Address*:";
  field.name = u"email$emailBox";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  FieldType expected_phone_number =
      base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber)
          ? PHONE_HOME_CITY_AND_NUMBER
          : PHONE_HOME_WHOLE_NUMBER;
  EXPECT_EQ(expected_phone_number, form_structure->field(9)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(10)->heuristic_type());
}

TEST_F(FormStructureTestImpl, ThreePartPhoneNumber) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Phone:";
  field.name = u"dayphone1";
  field.max_length = 0;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"-";
  field.name = u"dayphone2";
  field.max_length = 3;  // Size of prefix is 3.
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"-";
  field.name = u"dayphone3";
  field.max_length = 4;  // Size of suffix is 4.  If unlimited size is
                         // passed, phone will be parsed as
                         // <country code> - <area code> - <phone>.
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"ext.:";
  field.name = u"dayphone4";
  field.max_length = 0;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Name on Card";
  field.name = u"name_on_card";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = u"billing_address";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card Number";
  field.name = u"card_number";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration Date";
  field.name = u"expiration_month";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration Year";
  field.name = u"expiration_year";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Card number";
  field.name = u"ccnumber";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"First name";
  field.name = u"first_name";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last name";
  field.name = u"last_name";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration date";
  field.name = u"ccexpiresmonth";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"ccexpiresyear";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"cvc number";
  field.name = u"csc";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Cardholder Name";
  field.name = u"cc_first_name";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last name";
  field.name = u"last_name";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card number";
  field.name = u"ccnumber";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration date";
  field.name = u"ccexpiresmonth";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"ccexpiresyear";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"cvc number";
  field.name = u"csc";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

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
  field.form_control_type = FormControlType::kInputText;

  field.label = u"email";
  field.name = u"email";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"First Name";
  field.name = u"first";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Checkable fields shouldn't affect the signature.
  field.label = u"Select";
  field.name = u"Select";
  field.form_control_type = FormControlType::kInputCheckbox;
  field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);

  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(std::string("://&&email&first")),
            form_structure->FormSignatureAsStr());

  form.url = GURL(std::string("http://www.facebook.com"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("http://www.facebook.com&&email&first")),
            form_structure->FormSignatureAsStr());

  form.action = GURL(std::string("https://login.facebook.com/path"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("https://login.facebook.com&&email&first")),
            form_structure->FormSignatureAsStr());

  form.name = u"login_form";
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(std::string(
                "https://login.facebook.com&login_form&email&first")),
            form_structure->FormSignatureAsStr());

  // Checks how digits are removed from field names.
  field.check_status = FormFieldData::CheckStatus::kNotCheckable;
  field.label = u"Random Field label";
  field.name = u"random1234";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Random Field label2";
  field.name = u"random12345";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Random Field label3";
  field.name = u"1ran12dom12345678";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Random Field label3";
  field.name = u"12345ran123456dom123";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("https://login.facebook.com&login_form&email&first&"
                            "random1234&random&1ran12dom&random123")),
            form_structure->FormSignatureAsStr());
}

TEST_F(FormStructureTestImpl, CheckAlternativeFormSignatureLarge) {
  FormData large_form;
  large_form.url = GURL("http://foo.com/login?q=a#ref");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  large_form.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  large_form.fields.push_back(field2);

  FormFieldData field3;
  field3.form_control_type = FormControlType::kInputEmail;
  large_form.fields.push_back(field3);

  FormFieldData field4;
  field4.form_control_type = FormControlType::kInputTelephone;
  large_form.fields.push_back(field4);

  // Alternative form signature string of a form with more than two fields
  // should only concatenate scheme, host, and field types.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text&email&tel"),
            std::make_unique<FormStructure>(large_form)
                ->alternative_form_signature()
                .value());
}

TEST_F(FormStructureTestImpl, CheckAlternativeFormSignatureSmallPath) {
  FormData small_form_path;
  small_form_path.url = GURL("http://foo.com/login?q=a#ref");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  small_form_path.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  small_form_path.fields.push_back(field2);

  // Alternative form signature string of a form with 2 fields or less should
  // concatenate scheme, host, field types, and path if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text/login"),
            std::make_unique<FormStructure>(small_form_path)
                ->alternative_form_signature()
                .value());
}

TEST_F(FormStructureTestImpl, CheckAlternativeFormSignatureSmallRef) {
  FormData small_form_ref;
  small_form_ref.url = GURL("http://foo.com?q=a#ref");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  small_form_ref.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  small_form_ref.fields.push_back(field2);

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
  small_form_query.url = GURL("http://foo.com?q=a");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  small_form_query.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  small_form_query.fields.push_back(field2);

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
  form.name = u"the-name";
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");
  form.child_frames = {FrameTokenWithPredecessor()};

  FormFieldData field;
  field.label = u"username";
  field.name = u"username";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"password";
  field.name = u"password";
  field.form_control_type = FormControlType::kInputPassword;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = FormControlType::kInputText;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_TRUE(FormData::DeepEqual(form, FormStructure(form).ToFormData()));
}

// Tests that an Autofill upload for password form with 1 field should not be
// uploaded.
TEST_F(FormStructureTestImpl, OneFieldPasswordFormShouldNotBeUpload) {
  FormData form;
  FormFieldData field;
  field.name = u"Password";
  field.form_control_type = FormControlType::kInputPassword;
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormStructure(form).ShouldBeUploaded());
}


// Tests if a new logical form is started with the second appearance of a field
// of type |FieldTypeGroup::kName|.
TEST_F(FormStructureTestImpl, NoAutocompleteSectionNames) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;
  field.max_length = 10000;

  field.label = u"Full Name";
  field.name = u"fullName";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Full Name";
  field.name = u"fullName";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes({NAME_FULL, ADDRESS_HOME_COUNTRY, PHONE_HOME_NUMBER,
                      NAME_FULL, ADDRESS_HOME_COUNTRY, PHONE_HOME_NUMBER});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());
  EXPECT_EQ("fullName_0_11", form_structure.field(0)->section.ToString());
  EXPECT_EQ("fullName_0_11", form_structure.field(1)->section.ToString());
  EXPECT_EQ("fullName_0_11", form_structure.field(2)->section.ToString());
  EXPECT_EQ("fullName_0_14", form_structure.field(3)->section.ToString());
  EXPECT_EQ("fullName_0_14", form_structure.field(4)->section.ToString());
  EXPECT_EQ("fullName_0_14", form_structure.field(5)->section.ToString());
}

// Tests that the immediate recurrence of the |PHONE_HOME_NUMBER| type does not
// lead to a section split.
TEST_F(FormStructureTestImpl, NoSplitByRecurringPhoneFieldType) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  form.fields = {
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText, "", kFieldMaxLength),
      CreateTestFormField("Phone", "phone", "", FormControlType::kInputText, "",
                          kFieldMaxLength),
      CreateTestFormField("Mobile Number", "mobileNumber", "",
                          FormControlType::kInputText, "", kFieldMaxLength),
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText,
                          "section-blue billing name", kFieldMaxLength),
      CreateTestFormField("Phone", "phone", "", FormControlType::kInputText,
                          "section-blue billing tel", kFieldMaxLength),
      CreateTestFormField("Mobile Number", "mobileNumber", "",
                          FormControlType::kInputText,
                          "section-blue billing tel", kFieldMaxLength),
      CreateTestFormField("Country", "country", "", FormControlType::kInputText,
                          "", kFieldMaxLength)};
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes({NAME_FULL, PHONE_HOME_NUMBER, PHONE_HOME_NUMBER,
                      NAME_FULL, PHONE_HOME_NUMBER, PHONE_HOME_NUMBER,
                      ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(7U, form_structure.field_count());

  EXPECT_EQ("blue-billing", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(1)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(2)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(3)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(4)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(5)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(6)->section.ToString());
}

// Tests that adjacent name field types are not split into different sections.
TEST_F(FormStructureTestImpl, NoSplitAdjacentNameFieldType) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseParameterizedSectioning);

  FormData form;
  form.url = GURL("http://foo.com");
  form.fields = {CreateTestFormField("First Name", "firstname", "",
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
                                     FormControlType::kInputText)};
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes({NAME_FIRST, NAME_LAST, NAME_FIRST, NAME_LAST,
                      ADDRESS_HOME_COUNTRY, NAME_FIRST});

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());

  EXPECT_EQ(form_structure.field(0)->section, form_structure.field(1)->section);
  EXPECT_EQ(form_structure.field(0)->section, form_structure.field(2)->section);
  EXPECT_EQ(form_structure.field(0)->section, form_structure.field(3)->section);
  EXPECT_EQ(form_structure.field(0)->section, form_structure.field(4)->section);
  // The non-adjacent name field should be split into a different section.
  EXPECT_NE(form_structure.field(0)->section, form_structure.field(5)->section);
}

// Tests if a new logical form is started with the second appearance of a field
// of type |ADDRESS_HOME_COUNTRY|.
TEST_F(FormStructureTestImpl, SplitByRecurringFieldType) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseNewSectioningMethod);
  FormData form;
  form.url = GURL("http://foo.com");
  form.fields = {
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText,
                          "section-blue shipping name", kFieldMaxLength),
      CreateTestFormField("Country", "country", "", FormControlType::kInputText,
                          "section-blue shipping country", kFieldMaxLength),
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText,
                          "section-blue shipping name", kFieldMaxLength),
      CreateTestFormField("Country", "country", "", FormControlType::kInputText,
                          "", kFieldMaxLength)};
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-shipping", form_structure.field(1)->section.ToString());
  EXPECT_EQ("blue-shipping", form_structure.field(2)->section.ToString());
  EXPECT_EQ("country_2_14", form_structure.field(3)->section.ToString());
}

// Tests if a new logical form is started with the second appearance of a field
// of type |NAME_FULL| and another with the second appearance of a field of
// type |ADDRESS_HOME_COUNTRY|.
TEST_F(FormStructureTestImpl,
       SplitByNewAutocompleteSectionNameAndRecurringType) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kAutofillUseNewSectioningMethod);
  FormData form;
  form.url = GURL("http://foo.com");
  form.fields = {
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText,
                          "section-blue shipping name", kFieldMaxLength),
      CreateTestFormField("Country", "country", "", FormControlType::kInputText,
                          "section-blue billing country", kFieldMaxLength),
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText, "", kFieldMaxLength),
      CreateTestFormField("Country", "country", "", FormControlType::kInputText,
                          "", kFieldMaxLength)};
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(1)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(2)->section.ToString());
  EXPECT_EQ("country_2_14", form_structure.field(3)->section.ToString());
}  // namespace autofill

// Tests if a new logical form is started with the second appearance of a field
// of type |NAME_FULL|.
TEST_F(FormStructureTestImpl, SplitByNewAutocompleteSectionName) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  form.fields = {
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText,
                          "section-blue shipping name", kFieldMaxLength),
      CreateTestFormField("City", "city", "", FormControlType::kInputText, "",
                          kFieldMaxLength),
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText,
                          "section-blue billing name", kFieldMaxLength),
      CreateTestFormField("City", "city", "", FormControlType::kInputText, "",
                          kFieldMaxLength)};
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_CITY, NAME_FULL, ADDRESS_HOME_CITY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-shipping", form_structure.field(1)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(2)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(3)->section.ToString());
}

// Tests if a new logical form is started with the second appearance of a field
// of type |NAME_FULL|.
TEST_F(
    FormStructureTestImpl,
    FromEmptyAutocompleteSectionToDefinedOneWithSplitByNewAutocompleteSectionName) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  form.fields = {
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText, "", kFieldMaxLength),
      CreateTestFormField("Country", "country", "", FormControlType::kInputText,
                          "section-blue shipping country", kFieldMaxLength),
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText,
                          "section-blue billing name", kFieldMaxLength),
      CreateTestFormField("City", "city", "", FormControlType::kInputText, "",
                          kFieldMaxLength)};
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_CITY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-shipping", form_structure.field(1)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(2)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(3)->section.ToString());
}

// Tests if all the fields in the form belong to the same section when the
// second field has the autocomplete-section attribute set.
TEST_F(FormStructureTestImpl, FromEmptyAutocompleteSectionToDefinedOne) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  form.fields = {
      CreateTestFormField("Full Name", "fullName", "",
                          FormControlType::kInputText, "", kFieldMaxLength),
      CreateTestFormField("Country", "country", "", FormControlType::kInputText,
                          "section-blue shipping country", kFieldMaxLength)};
  FormStructure form_structure(form);
  test_api(form_structure).SetFieldTypes({NAME_FULL, ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(2U, form_structure.field_count());

  EXPECT_EQ("blue-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-shipping", form_structure.field(1)->section.ToString());
}

// Tests if all the fields in the form belong to the same section when one of
// the field is ignored.
TEST_F(FormStructureTestImpl,
       FromEmptyAutocompleteSectionToDefinedOneWithIgnoredField) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  form.fields.push_back(CreateTestFormField("Full Name", "fullName", "",
                                            FormControlType::kInputText, "",
                                            kFieldMaxLength));
  form.fields.push_back(CreateTestFormField(
      "Phone", "phone", "", FormControlType::kInputText, "", kFieldMaxLength));
  form.fields.back().is_focusable = false;  // hidden
  form.fields.push_back(CreateTestFormField("Full Name", "fullName", "",
                                            FormControlType::kInputText,
                                            "shipping name", kFieldMaxLength));
  FormStructure form_structure(form);
  test_api(form_structure)
      .SetFieldTypes({NAME_FULL, PHONE_HOME_NUMBER, NAME_FULL});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(3U, form_structure.field_count());

  EXPECT_EQ("-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("-shipping", form_structure.field(1)->section.ToString());
  EXPECT_EQ("-shipping", form_structure.field(2)->section.ToString());
}

TEST_F(FormStructureTestImpl, FindFieldsEligibleForManualFilling) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;
  field.max_length = 10000;

  field.label = u"Full Name";
  field.name = u"fullName";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  FieldGlobalId full_name_id = field.global_id();

  field.label = u"Country";
  field.name = u"country";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Unknown";
  field.name = u"unknown";
  field.renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  FieldGlobalId unknown_id = field.global_id();

  FormStructure form_structure(form);

  test_api(form_structure)
      .SetFieldTypes(
          {CREDIT_CARD_NAME_FULL, ADDRESS_HOME_COUNTRY, UNKNOWN_TYPE});

  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  test_api(form_structure).IdentifySections(/*ignore_autocomplete=*/false);
  std::vector<FieldGlobalId> expected_result;
  // Only credit card related and unknown fields are eligible for manual
  // filling.
  expected_result.push_back(full_name_id);
  expected_result.push_back(unknown_id);

  EXPECT_EQ(expected_result,
            FormStructure::FindFieldsEligibleForManualFilling(forms));
}

// Tests that AssignBestFieldTypes() sets (only) the PatternSource.
TEST_P(FormStructureTest_ForPatternSource, ParseFieldTypesWithPatterns) {
  FormData form = test::CreateTestAddressFormData();
  FormStructure form_structure(form);
  ParsingContext context(GeoIpCountryCode(""), LanguageCode(""),
                         pattern_source());
  test_api(form_structure)
      .AssignBestFieldTypes(
          test_api(form_structure).ParseFieldTypesWithPatterns(context),
          pattern_source());
  ASSERT_THAT(form_structure.fields(), Not(IsEmpty()));

  auto get_heuristic_type = [&](const AutofillField& field) {
    return field.heuristic_type(
        PatternSourceToHeuristicSource(pattern_source()));
  };
  EXPECT_THAT(
      form_structure.fields(),
      Each(Pointee(ResultOf(get_heuristic_type,
                            AllOf(Not(NO_SERVER_DATA), Not(UNKNOWN_TYPE))))));

  for (PatternSource other_pattern_source : other_pattern_sources()) {
    auto get_other_pattern_heuristic_type = [&](const AutofillField& field) {
      return field.heuristic_type(
          PatternSourceToHeuristicSource(other_pattern_source));
    };
    EXPECT_THAT(form_structure.fields(),
                Each(Pointee(ResultOf(get_other_pattern_heuristic_type,
                                      NO_SERVER_DATA))))
        << "PatternSource = " << static_cast<int>(other_pattern_source);
  }
}

TEST_F(FormStructureTestImpl, DetermineRanks) {
  FormData form;
  form.url = GURL("http://foo.com");

  auto add_field = [&form](const std::u16string& name,
                           LocalFrameToken frame_token,
                           FormRendererId host_form_id) {
    FormFieldData field;
    field.form_control_type = FormControlType::kInputText;
    field.name = name;
    field.renderer_id = test::MakeFieldRendererId();
    field.host_frame = frame_token;
    field.host_form_id = host_form_id;
    form.fields.push_back(field);
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
  for (FormFieldData& field : form.fields) {
    field.parsed_autocomplete =
        AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized};
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

}  // namespace autofill
