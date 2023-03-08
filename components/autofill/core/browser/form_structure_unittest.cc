// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stddef.h>

#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/invoke.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::base::ASCIIToUTF16;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;
using ::version_info::GetProductNameAndVersionForUserAgent;

namespace autofill {

using FieldPrediction = ::autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;
using autofill::features::kAutofillLabelAffixRemoval;
using autofill::mojom::SubmissionIndicatorEvent;
using autofill::mojom::SubmissionSource;
using autofill::test::AddFieldPredictionsToForm;
using autofill::test::AddFieldPredictionToForm;
using autofill::test::CreateFieldPrediction;

namespace {

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  std::string response_string;
  base::Base64Encode(unencoded_response_string, &response_string);
  return response_string;
}

void AddFieldOverrideToForm(
    autofill::FormFieldData field_data,
    ServerFieldType field_type,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion) {
  AddFieldPredictionsToForm(
      field_data,
      {CreateFieldPrediction(field_type, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
}

// Matches any protobuf `actual` whose serialization is equal to the
// string-serialization of the protobuf `expected`.
template <typename T>
auto SerializesSameAs(const T& expected) {
  std::string expected_string;
  CHECK(expected.SerializeToString(&expected_string));
  return Truly([expected_string](const auto& actual) {
    std::string actual_string;
    CHECK(actual.SerializeToString(&actual_string));
    return actual_string == expected_string;
  });
}

template <typename... Matchers>
auto ElementsSerializeSameAs(Matchers... element_matchers) {
  return ElementsAre(SerializesSameAs(element_matchers)...);
}

template <typename... Matchers>
auto UnorderedElementsSerializeSameAs(Matchers... element_matchers) {
  return UnorderedElementsAre(SerializesSameAs(element_matchers)...);
}

FormStructureTestApi test_api(FormStructure* form_structure) {
  return FormStructureTestApi(form_structure);
}

constexpr DenseSet<PatternSource> kAllPatternSources {
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  PatternSource::kLegacy
#else
  PatternSource::kLegacy, PatternSource::kDefault, PatternSource::kExperimental,
      PatternSource::kNextGen
#endif
};

Matcher<FieldPrediction> EqualsPrediction(const FieldPrediction& prediction) {
  return AllOf(
      Property("type", &FieldPrediction::type, prediction.type()),
      Property("source", &FieldPrediction::source, prediction.source()));
}

Matcher<FieldPrediction> EqualsPrediction(ServerFieldType type) {
  return Property("type", &FieldPrediction::type, type);
}

}  // namespace

class FormStructureTestImpl : public test::FormStructureTest {
 public:
  static std::string Hash64Bit(const std::string& str) {
    return base::NumberToString(StrToHash64Bit(str));
  }

 protected:
  bool FormIsAutofillable(const FormData& form) {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
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

class ParameterizedFormStructureTest
    : public FormStructureTestImpl,
      public testing::WithParamInterface<bool> {};

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
  CheckFormStructureTestData({{{.description_for_logging = "FieldCount",
                                .fields = {{.role = ServerFieldType::USERNAME},
                                           {.label = u"Password",
                                            .name = u"password",
                                            .form_control_type = "password"},
                                           {.label = u"Submit",
                                            .name = u"",
                                            .form_control_type = "submit"},
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
         .fields = {{.role = ServerFieldType::USERNAME},
                    {.label = u"Password",
                     .name = u"password",
                     .form_control_type = "password"},
                    {.role = ServerFieldType::EMAIL_ADDRESS},
                    {.role = ServerFieldType::ADDRESS_HOME_CITY},
                    {.role = ServerFieldType::ADDRESS_HOME_STATE,
                     .form_control_type = "select-one"},
                    {.label = u"Submit",
                     .name = u"",
                     .form_control_type = "submit"}}},
        {
            .determine_heuristic_type = true,
            .autofill_count = 3,
        },
        {}},

       {{.description_for_logging = "AutofillCountWithNonFillableField",
         .fields =
             {{.role = ServerFieldType::USERNAME},
              {.label = u"Password",
               .name = u"password",
               .form_control_type = "password"},
              {.role = ServerFieldType::EMAIL_ADDRESS},
              {.role = ServerFieldType::ADDRESS_HOME_CITY},
              {.role = ServerFieldType::ADDRESS_HOME_STATE,
               .form_control_type = "select-one"},
              {.label = u"Submit", .name = u"", .form_control_type = "submit"},
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
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // With min required fields enabled.
  EXPECT_FALSE(FormIsAutofillable(form));

  // Add a password field. The form should be picked up by the password but
  // not by autofill.
  field.label = u"password";
  field.name = u"password";
  field.form_control_type = "password";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.label = u"Full Name";
  field.name = u"fullname";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.label = u"Address Line 1";
  field.name = u"address1";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form));

  // We now have three auto-fillable fields. It's always autofillable.
  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = "email";
  field.unique_renderer_id = test::MakeFieldRendererId();
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
    field.unique_renderer_id = test::MakeFieldRendererId();
    form_.fields.push_back(std::move(field));
    form_structure_ = nullptr;
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
    field.form_control_type = "radio";
    AddField(field);
  }
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));

  // Add a second checkable field.
  {
    FormFieldData field;
    field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
    field.form_control_type = "checkbox";
    AddField(field);
  }
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));

  // Add one text field.
  {
    FormFieldData field;
    field.form_control_type = "text";
    AddField(field);
  }
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
}

// Forms with at least one text field should be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, TrueIfOneTextField) {
  {
    FormFieldData field;
    field.form_control_type = "text";
    AddField(field);
  }
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));

  {
    FormFieldData field;
    field.form_control_type = "text";
    AddField(field);
  }
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
    field.form_control_type = "select-one";
    AddField(field);
  }
  EXPECT_FALSE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));

  {
    FormFieldData field;
    field.form_control_type = "text";
    AddField(field);
  }
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 1}));
  EXPECT_TRUE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));
}

// Form whose action is a search URL should not be parsed.
TEST_F(FormStructureTestImpl_ShouldBeParsed_Test, FalseIfSearchURL) {
  {
    FormFieldData field;
    field.form_control_type = "text";
    AddField(field);
  }
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
    field.form_control_type = "password";
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
    field.form_control_type = "password";
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
  {
    FormFieldData field;
    field.form_control_type = "text";
    AddField(field);
  }
  EXPECT_TRUE(test_api(form_structure()).ShouldBeParsed());
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));
  EXPECT_FALSE(
      test_api(form_structure()).ShouldBeParsed({.min_required_fields = 2}));

  {
    FormFieldData field;
    field.parsed_autocomplete = AutocompleteParsingResult{
        .section = "my-billing-section", .field_type = HtmlFieldType::kName};
    field.form_control_type = "text";
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
  FormFieldData field;

  test::CreateTestFormField("Name", "name", "", "text", "name", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Email", "email", "", "text", "email", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Address", "address", "", "text", "address-line1",
                            &field);
  form.fields.push_back(field);

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
  FormFieldData field;

  test::CreateTestFormField("Name", "name", "", "text", "name", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Address", "Address", "", "select-one", "", &field);
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(form_structure->ShouldBeParsed());
}

// Tests that unmappable autocomplete values containing "address" are treated
// as HtmlFieldType::kUnspecified instead of
// HtmlFieldType::kUnrecognized.
TEST_F(FormStructureTestImpl, IgnoreUnmappableAutocompleteValues) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillIgnoreUnmappableAutocompleteValues);

  CheckFormStructureTestData(
      {{{.description_for_logging = "IgnoreUnmappableAutocompleteValues",
         .fields = {{.autocomplete_attribute = "address-info"}}},
        {.determine_heuristic_type = true},
        {.expected_html_type = {HtmlFieldType::kUnspecified}}}});
}

// Tests that ShouldBeParsed returns true for a form containing less than three
// fields if at least one has an autocomplete attribute.
TEST_F(FormStructureTestImpl, DetermineHeuristicTypes_AutocompleteFalse) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "DetermineHeuristicTypes_AutocompleteFalse",
         .fields = {{.label = u"Name",
                     .name = u"name",
                     .autocomplete_attribute = "false",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "false", /*field_max_length=*/0)},
                    {.role = ServerFieldType::EMAIL_ADDRESS,
                     .autocomplete_attribute = "false",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "false", /*field_max_length=*/0)},
                    {.role = ServerFieldType::ADDRESS_HOME_STATE,
                     .autocomplete_attribute = "false",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "false", /*field_max_length=*/0),
                     .form_control_type = "select-one"}}},
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
         .fields = {{.role = ServerFieldType::NAME_FIRST},
                    {.role = ServerFieldType::NAME_LAST},
                    {.role = ServerFieldType::EMAIL_ADDRESS},
                    {.role = ServerFieldType::PHONE_HOME_NUMBER},
                    {.label = u"Ext:", .name = u"phoneextension"},
                    {.label = u"Address", .name = u"address"},
                    {.role = ServerFieldType::ADDRESS_HOME_CITY},
                    {.role = ServerFieldType::ADDRESS_HOME_ZIP},
                    {.label = u"Submit",
                     .name = u"",
                     .form_control_type = "submit"}}},
        {
            .determine_heuristic_type = true,
            .field_count = 9,
            .autofill_count = 8,
        },
        {.expected_heuristic_type = {
             NAME_FIRST, NAME_LAST, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER,
             PHONE_HOME_EXTENSION, ADDRESS_HOME_LINE1, ADDRESS_HOME_CITY,
             ADDRESS_HOME_ZIP, UNKNOWN_TYPE}}}});
}

// Verify that we can correctly process the |autocomplete| attribute.
TEST_F(FormStructureTestImpl, HeuristicsAutocompleteAttribute) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "HeuristicsAutocompleteAttribute",
         .fields = {{.label = u"",
                     .name = u"field1",
                     .autocomplete_attribute = "given-name",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "given-name", /*field_max_length=*/0)},
                    {.label = u"",
                     .name = u"field2",
                     .autocomplete_attribute = "family-name",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "family-name", /*field_max_length=*/0)},
                    {.label = u"",
                     .name = u"field3",
                     .autocomplete_attribute = "email",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "email", /*field_max_length=*/0)},
                    {.label = u"",
                     .name = u"field4",
                     .autocomplete_attribute = "upi-vpa",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "upi-vpa", /*field_max_length=*/0)}}},
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
  CheckFormStructureTestData(
      {{{.description_for_logging = "StripCommonNamePrefix",
         .fields =
             {{.role = ServerFieldType::NAME_FIRST,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$firstname"},
              {.role = ServerFieldType::NAME_LAST,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$lastname"},
              {.role = ServerFieldType::EMAIL_ADDRESS,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$email"},
              {.role = ServerFieldType::PHONE_HOME_NUMBER,
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$phone"},
              {.label = u"Submit",
               .name = u"ctl01$ctl00$ShippingAddressCreditPhone$submit",
               .form_control_type = "submit"}}},
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .field_count = 5,
         .autofill_count = 4},
        {.expected_heuristic_type = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                     PHONE_HOME_WHOLE_NUMBER, UNKNOWN_TYPE}}}});
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
         .fields = {{.role = ServerFieldType::CREDIT_CARD_NUMBER},
                    {.label = u"Expiration", .name = u"cc_exp"},
                    {.role = ServerFieldType::ADDRESS_HOME_ZIP}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = true},
        {}}});
}

TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_Full) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_Full",
         .fields = {{.label = u"Name on Card", .name = u"name_on_card"},
                    {.role = ServerFieldType::CREDIT_CARD_NUMBER},
                    {.label = u"Exp Month", .name = u"ccmonth"},
                    {.label = u"Exp Year", .name = u"ccyear"},
                    {.label = u"Verification", .name = u"verification"},
                    {.label = u"Submit",
                     .name = u"submit",
                     .form_control_type = "submit"}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = true},
        {}}});
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_OnlyCCNumber) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_OnlyCCNumber",
         .fields = {{.role = ServerFieldType::CREDIT_CARD_NUMBER}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = false},
        {}}});
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_AddressForm) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_AddressForm",
         .fields = {{.role = ServerFieldType::NAME_FIRST, .name = u""},
                    {.role = ServerFieldType::NAME_LAST, .name = u""},
                    {.role = ServerFieldType::EMAIL_ADDRESS, .name = u""},
                    {.role = ServerFieldType::PHONE_HOME_NUMBER, .name = u""},
                    {.label = u"Address", .name = u""},
                    {.label = u"Address", .name = u""},
                    {.role = ServerFieldType::ADDRESS_HOME_ZIP, .name = u""}}},
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
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "tel-local", /*field_max_length=*/0)},
                    {.label = u"",
                     .name = u"field2",
                     .autocomplete_attribute = "tel-local-prefix",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "tel-local-prefix", /*field_max_length=*/0)},
                    {.label = u"",
                     .name = u"field3",
                     .autocomplete_attribute = "tel-local-suffix",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "tel-local-suffix", /*field_max_length=*/0)}}},
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
         .fields = {{.role = ServerFieldType::NAME_FIRST},
                    {.role = ServerFieldType::NAME_LAST},
                    {.role = ServerFieldType::EMAIL_ADDRESS}}},
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
         .fields = {{.role = ServerFieldType::NAME_FIRST,
                     .autocomplete_attribute = "given-name",
                     .parsed_autocomplete = ParseAutocompleteAttribute(
                         "given-name", /*field_max_length=*/0)},
                    {.role = ServerFieldType::NAME_LAST},
                    {.role = ServerFieldType::EMAIL_ADDRESS}}},
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
            .fields = {{.role = ServerFieldType::NAME_FIRST,
                        .autocomplete_attribute = "unrecognized",
                        .parsed_autocomplete = ParseAutocompleteAttribute(
                            "unrecognized", /*field_max_length=*/0)},
                       {.label = u"Middle Name", .name = u"middlename"},
                       {.role = ServerFieldType::NAME_LAST},
                       {.role = ServerFieldType::EMAIL_ADDRESS}},
        },
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .should_be_queried = true,
         .field_count = 4,
         .autofill_count = 3},
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
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = u"firstname";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormShouldRunHeuristics(form));

  EXPECT_TRUE(FormShouldBeQueried(form));

  // Default configuration.
  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
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

  FormFieldData field;

  // Set a valid autocomplete attribute to the first field.
  test::CreateTestFormField("First Name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lastname", "", "text", "", &field);
  form.fields.push_back(field);

  EXPECT_FALSE(FormShouldRunHeuristics(form));

  EXPECT_TRUE(FormShouldBeQueried(form));

  // As a side effect of parsing small forms (if any of the heuristics, query,
  // or upload minimums are disabled, we'll autofill fields with an
  // autocomplete attribute, even if its the only field in the form.
  {
    FormData form_copy = form;
    form_copy.fields.pop_back();
    FormStructure form_structure(form_copy);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Promo Code";
  field.name = u"promocode";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_TRUE(FormShouldRunHeuristicsForSingleFieldForms(form));

  // Default configuration.
  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes(nullptr, nullptr);
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

  // Start with a regular contact form.
  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Email", "email", "", "text", "username", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Password", "Password", "", "password", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
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

  FormFieldData field;

  // Some fields will have no section specified.  These fall into the default
  // section.
  test::CreateTestFormField("", "", "", "text", "email", &field);
  form.fields.push_back(field);

  // We allow arbitrary section names.
  test::CreateTestFormField("", "", "", "text", "section-foo email", &field);
  form.fields.push_back(field);

  // "shipping" and "billing" are special section tokens that don't require the
  // "section-" prefix.
  test::CreateTestFormField("", "", "", "text", "shipping email", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "billing email", &field);
  form.fields.push_back(field);

  // "shipping" and "billing" can be combined with other section names.
  test::CreateTestFormField("", "", "", "text", "section-foo shipping email",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "section-foo billing email",
                            &field);
  form.fields.push_back(field);

  // We don't do anything clever to try to coalesce sections; it's up to site
  // authors to avoid typos.
  test::CreateTestFormField("", "", "", "text", "section--foo email", &field);
  form.fields.push_back(field);

  // "shipping email" and "section--shipping" email should be parsed as
  // different sections.  This is only an interesting test due to how we
  // implement implicit section names from attributes like "shipping email"; see
  // the implementation for more details.
  test::CreateTestFormField("", "", "", "text", "section--shipping email",
                            &field);
  form.fields.push_back(field);

  // Credit card fields are implicitly in one, separate credit card section.
  test::CreateTestFormField("", "", "", "text", "section-foo cc-number",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);
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

  FormFieldData field;

  // Some fields will have no section specified.  These fall into the default
  // section.
  test::CreateTestFormField("", "", "", "text", "email", &field);
  form.fields.push_back(field);

  // Specifying "section-" is equivalent to not specifying a section.
  test::CreateTestFormField("", "", "", "text", "section- email", &field);
  form.fields.push_back(field);

  // Invalid tokens should prevent us from setting a section name.
  test::CreateTestFormField("", "", "", "text", "garbage section-foo email",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "garbage section-bar email",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "garbage shipping email",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "garbage billing email",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

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

  FormFieldData field;

  test::CreateTestFormField("", "", "", "text", "section-foo email", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "section-foo address-line1",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

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

  FormFieldData field;

  test::CreateTestFormField("", "one", "", "text", "address-line1", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "section-foo email", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "", "", "text", "name", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "two", "", "text", "address-line1", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

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
  field.form_control_type = "text";

  field.label = u"Your First Name:";
  field.name = u"bill.first";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Your Last Name:";
  field.name = u"bill.last";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Street Address Line 1:";
  field.name = u"bill.street1";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Street Address Line 2:";
  field.name = u"bill.street2";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City";
  field.name = u"bill.city";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"State (U.S.):";
  field.name = u"bill.state";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Zip/Postal Code:";
  field.name = u"BillTo.PostalCode";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country:";
  field.name = u"bill.country";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone Number:";
  field.name = u"BillTo.Phone";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = "submit";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(8)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(9)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsSample6) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"E-mail address";
  field.name = u"email";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Full name";
  field.name = u"name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Company";
  field.name = u"company";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = u"address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City";
  field.name = u"city";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Zip Code";
  field.name = u"Home.PostalCode";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.value = u"continue";
  field.form_control_type = "submit";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = std::u16string();
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = std::u16string();
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = std::u16string();
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = std::u16string();
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = std::u16string();
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = std::u16string();
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Zip code";
  field.name = std::u16string();
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = "submit";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
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
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"Name on Card";
  field.name = u"name_on_card";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card Number";
  field.name = u"card_number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Month";
  field.name = u"ccmonth";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Year";
  field.name = u"ccyear";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Verification";
  field.name = u"verification";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = "submit";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Name on Card";
  field.name = u"name_on_card";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // This is not a field we know how to process.  But we should skip over it
  // and process the other fields in the card block.
  field.label = u"Card image";
  field.name = u"card_image";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card Number";
  field.name = u"card_number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Month";
  field.name = u"ccmonth";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Exp Year";
  field.name = u"ccyear";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Verification";
  field.name = u"verification";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = "submit";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Address Line1";
  field.name = u"Address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line2";
  field.name = u"Address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line3";
  field.name = u"Address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City";
  field.name = u"city";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Address Line1";
  field.name = u"shipping.address.addressLine1";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line2";
  field.name = u"shipping.address.addressLine2";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line3";
  field.name = u"billing.address.addressLine3";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line4";
  field.name = u"billing.address.addressLine4";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Street:";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_ads1";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Suite or Apt:";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_adap";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Street address second line";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_ads2";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City:";
  field.name = u"FOPIH_RgWebCC_0_IHAddress_adct";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Address Line1";
  field.name = u"address1";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Floor number, suite number, etc";
  field.name = u"address2";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City:";
  field.name = u"city";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Address Line1";
  field.name = u"Address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address Line2";
  field.name = u"Address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"State/Province/Region";
  field.name = u"State";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"First Name*:";
  field.name = u"editBillingAddress$firstNameBox";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name*:";
  field.name = u"editBillingAddress$lastNameBox";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Company Name:";
  field.name = u"editBillingAddress$companyBox";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address*:";
  field.name = u"editBillingAddress$addressLine1Box";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Apt/Suite :";
  field.name = u"editBillingAddress$addressLine2Box";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"City*:";
  field.name = u"editBillingAddress$cityBox";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"State/Province*:";
  field.name = u"editBillingAddress$stateDropDown";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country*:";
  field.name = u"editBillingAddress$countryDropDown";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Postal Code*:";
  field.name = u"editBillingAddress$zipCodeBox";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone*:";
  field.name = u"editBillingAddress$phoneBox";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email Address*:";
  field.name = u"email$emailBox";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(9)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(10)->heuristic_type());
}

TEST_F(FormStructureTestImpl, ThreePartPhoneNumber) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"Phone:";
  field.name = u"dayphone1";
  field.max_length = 0;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"-";
  field.name = u"dayphone2";
  field.max_length = 3;  // Size of prefix is 3.
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"-";
  field.name = u"dayphone3";
  field.max_length = 4;  // Size of suffix is 4.  If unlimited size is
                         // passed, phone will be parsed as
                         // <country code> - <area code> - <phone>.
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"ext.:";
  field.name = u"dayphone4";
  field.max_length = 0;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Name on Card";
  field.name = u"name_on_card";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = u"billing_address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card Number";
  field.name = u"card_number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration Date";
  field.name = u"expiration_month";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration Year";
  field.name = u"expiration_year";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Card number";
  field.name = u"ccnumber";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"First name";
  field.name = u"first_name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last name";
  field.name = u"last_name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration date";
  field.name = u"ccexpiresmonth";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"ccexpiresyear";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"cvc number";
  field.name = u"csc";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  field.form_control_type = "text";

  field.label = u"Cardholder Name";
  field.name = u"cc_first_name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last name";
  field.name = u"last_name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Card number";
  field.name = u"ccnumber";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration date";
  field.name = u"ccexpiresmonth";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"ccexpiresyear";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"cvc number";
  field.name = u"csc";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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

TEST_P(ParameterizedFormStructureTest, EncodeQueryRequest) {
  bool autofill_across_iframes = GetParam();
  base::test::ScopedFeatureList scoped_features;
  std::vector<base::test::FeatureRef> enabled;
  std::vector<base::test::FeatureRef> disabled;
  (autofill_across_iframes ? &enabled : &disabled)
      ->push_back(features::kAutofillAcrossIframes);
  scoped_features.InitWithFeatures(enabled, disabled);

  FormSignature form_signature(16692857476255362434UL);

  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"Name on Card";
  field.name = u"name_on_card";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = form_signature;
  form.fields.push_back(field);

  field.label = u"Address";
  field.name = u"billing_address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(12345UL);
  form.fields.push_back(field);

  field.label = u"Card Number";
  field.name = u"card_number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(67890UL);
  form.fields.push_back(field);

  field.label = u"Expiration Date";
  field.name = u"expiration_month";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(12345UL);
  form.fields.push_back(field);

  field.label = u"Expiration Year";
  field.name = u"expiration_year";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(12345UL);
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  checkable_field.host_form_signature = form_signature;
  form.fields.push_back(checkable_field);

  FormStructure form_structure(form);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  std::vector<FormSignature> expected_signatures;
  expected_signatures.push_back(FormSignature(form_signature.value()));
  if (autofill_across_iframes) {
    expected_signatures.push_back(FormSignature(12345UL));
    expected_signatures.push_back(FormSignature(67890UL));
  }

  // Prepare the expected proto string.
  AutofillPageQueryRequest query;
  query.set_client_version(GetProductNameAndVersionForUserAgent());
  {
    AutofillPageQueryRequest::Form* query_form = query.add_forms();
    query_form->set_signature(form_signature.value());
    query_form->add_fields()->set_signature(412125936U);
    query_form->add_fields()->set_signature(1917667676U);
    query_form->add_fields()->set_signature(2226358947U);
    query_form->add_fields()->set_signature(747221617U);
    query_form->add_fields()->set_signature(4108155786U);
    if (autofill_across_iframes) {
      query_form = query.add_forms();
      query_form->set_signature(12345UL);
      query_form->add_fields()->set_signature(1917667676U);
      query_form->add_fields()->set_signature(747221617U);
      query_form->add_fields()->set_signature(4108155786U);

      query_form = query.add_forms();
      query_form->set_signature(67890UL);
      query_form->add_fields()->set_signature(2226358947U);
    }
  }

  AutofillPageQueryRequest encoded_query;
  std::vector<FormSignature> encoded_signatures;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  EXPECT_EQ(encoded_signatures, expected_signatures);
  EXPECT_THAT(encoded_query, SerializesSameAs(query));

  // Add the same form, only one will be encoded, so EncodeQueryRequest() should
  // return the same data.
  FormStructure form_structure2(form);
  forms.push_back(&form_structure2);

  std::vector<FormSignature> expected_signatures2 = expected_signatures;

  AutofillPageQueryRequest encoded_query2;
  std::vector<FormSignature> encoded_signatures2;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query2,
                                                &encoded_signatures2));
  EXPECT_EQ(encoded_signatures2, expected_signatures2);
  EXPECT_THAT(encoded_query2, SerializesSameAs(query));

  // Add 5 address fields - this should be still a valid form.
  FormSignature form_signature3(2608858059775241169UL);
  for (auto& f : form.fields) {
    if (f.host_form_signature == form_signature)
      f.host_form_signature = form_signature3;
  }
  for (size_t i = 0; i < 5; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.unique_renderer_id = test::MakeFieldRendererId();
    field.host_form_signature = form_signature3;
    form.fields.push_back(field);
  }

  FormStructure form_structure3(form);
  forms.push_back(&form_structure3);

  std::vector<FormSignature> expected_signatures3 = expected_signatures2;
  expected_signatures3.push_back(form_signature3);

  // Add the second form to the expected proto.
  {
    AutofillPageQueryRequest::Form* query_form = query.add_forms();
    query_form->set_signature(2608858059775241169);
    query_form->add_fields()->set_signature(412125936U);
    query_form->add_fields()->set_signature(1917667676U);
    query_form->add_fields()->set_signature(2226358947U);
    query_form->add_fields()->set_signature(747221617U);
    query_form->add_fields()->set_signature(4108155786U);
    for (int i = 0; i < 5; ++i)
      query_form->add_fields()->set_signature(509334676U);
  }

  AutofillPageQueryRequest encoded_query3;
  std::vector<FormSignature> encoded_signatures3;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query3,
                                                &encoded_signatures3));
  EXPECT_EQ(encoded_signatures3, expected_signatures3);
  EXPECT_THAT(encoded_query3, SerializesSameAs(query));

  // |form_structures4| will have the same signature as |form_structure3|.
  form.fields.back().name = u"address123456789";

  FormStructure form_structure4(form);
  forms.push_back(&form_structure4);

  std::vector<FormSignature> expected_signatures4 = expected_signatures3;

  AutofillPageQueryRequest encoded_query4;
  std::vector<FormSignature> encoded_signatures4;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query4,
                                                &encoded_signatures4));
  EXPECT_EQ(encoded_signatures4, expected_signatures4);
  EXPECT_THAT(encoded_query4, SerializesSameAs(query));

  FormData malformed_form(form);
  // Add 300 address fields - the form is not valid anymore, but previous ones
  // are. The result should be the same as in previous test.
  for (size_t i = 0; i < 300; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.unique_renderer_id = test::MakeFieldRendererId();
    malformed_form.fields.push_back(field);
  }

  FormStructure malformed_form_structure(malformed_form);
  forms.push_back(&malformed_form_structure);

  std::vector<FormSignature> expected_signatures5 = expected_signatures4;

  AutofillPageQueryRequest encoded_query5;
  std::vector<FormSignature> encoded_signatures5;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query5,
                                                &encoded_signatures5));
  EXPECT_EQ(encoded_signatures5, expected_signatures5);
  EXPECT_THAT(encoded_query5, SerializesSameAs(query));

  // Check that we fail if there are only bad form(s).
  std::vector<FormStructure*> bad_forms;
  bad_forms.push_back(&malformed_form_structure);
  AutofillPageQueryRequest encoded_query6;
  std::vector<FormSignature> encoded_signatures6;
  EXPECT_FALSE(FormStructure::EncodeQueryRequest(bad_forms, &encoded_query6,
                                                 &encoded_signatures6));
}

TEST_F(FormStructureTestImpl,
       EncodeUploadRequest_SubmissionIndicatorEvents_Match) {
  // Statically assert that the mojo SubmissionIndicatorEvent enum matches the
  // corresponding entries the in proto AutofillUploadContents
  // SubmissionIndicatorEvent enum.
  static_assert(AutofillUploadContents::NONE ==
                    static_cast<int>(SubmissionIndicatorEvent::NONE),
                "NONE enumerator does not match!");
  static_assert(
      AutofillUploadContents::HTML_FORM_SUBMISSION ==
          static_cast<int>(SubmissionIndicatorEvent::HTML_FORM_SUBMISSION),
      "HTML_FORM_SUBMISSION enumerator does not match!");
  static_assert(
      AutofillUploadContents::SAME_DOCUMENT_NAVIGATION ==
          static_cast<int>(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION),
      "SAME_DOCUMENT_NAVIGATION enumerator does not match!");
  static_assert(AutofillUploadContents::XHR_SUCCEEDED ==
                    static_cast<int>(SubmissionIndicatorEvent::XHR_SUCCEEDED),
                "XHR_SUCCEEDED enumerator does not match!");
  static_assert(AutofillUploadContents::FRAME_DETACHED ==
                    static_cast<int>(SubmissionIndicatorEvent::FRAME_DETACHED),
                "FRAME_DETACHED enumerator does not match!");
  static_assert(
      AutofillUploadContents::DOM_MUTATION_AFTER_XHR ==
          static_cast<int>(SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR),
      "DOM_MUTATION_AFTER_XHR enumerator does not match!");
  static_assert(AutofillUploadContents::
                        PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD ==
                    static_cast<int>(
                        SubmissionIndicatorEvent::
                            PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD),
                "PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD enumerator "
                "does not match!");
  static_assert(
      AutofillUploadContents::PROBABLE_FORM_SUBMISSION ==
          static_cast<int>(SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION),
      "PROBABLE_FORM_SUBMISSION enumerator does not match!");
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

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithMatchingValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::UNVALIDATED});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::UNVALIDATED});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::INVALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER}, {AutofillProfile::EMPTY});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLetter, true));
  form_structure->set_password_length_vote(10u);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_passwords_revealed(false);
  upload.set_password_has_letter(true);
  upload.set_password_length(10u);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        nullptr, 3U, 0);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        nullptr, 5U, 0);
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        nullptr, 9U, 3);
  test::FillUploadField(upload.add_field(), 466116101U, "phone", "number",
                        nullptr, 14U, 1);
  test::FillUploadField(upload.add_field(), 2799270304U, "country",
                        "select-one", nullptr, 36U, 2);

  ////////////////
  // Verification
  ////////////////
  std::string expected_upload_string;
  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  upload.set_autofill_used(true);
  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  ////////////////
  // Setup
  ////////////////
  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.form_control_type = "text";
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2},
        {AutofillProfile::VALID, AutofillProfile::VALID});
  }

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLetter, true));
  form_structure->set_password_length_vote(10u);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Adjust the expected proto string.
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  // Create an additional 2 fields (total of 7).  Put the appropriate autofill
  // type on the different address fields.
  test::FillUploadField(upload.add_field(), 509334676U, "address", "text",
                        nullptr, {30U, 31U}, {2, 2});
  test::FillUploadField(upload.add_field(), 509334676U, "address", "text",
                        nullptr, {30U, 31U}, {2, 2});

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithNonMatchingValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::UNVALIDATED});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::UNVALIDATED});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::INVALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER}, {AutofillProfile::EMPTY});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLetter, true));
  form_structure->set_password_length_vote(10u);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_passwords_revealed(false);
  upload.set_password_has_letter(true);
  upload.set_password_length(10u);
  upload.set_action_signature(15724779818122431245U);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        nullptr, 3U, 0);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        nullptr, 5U, 0);
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        nullptr, 9U, 3);
  test::FillUploadField(upload.add_field(), 466116101U, "phone", "number",
                        nullptr, 14U, 1);
  test::FillUploadField(upload.add_field(), 2799270304U, "country",
                        "select-one", nullptr, 36U,
                        1);  // Non-matching validities

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsAre(Not(SerializesSameAs(upload))));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithMultipleValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::UNVALIDATED, AutofillProfile::VALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::UNVALIDATED, AutofillProfile::VALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::INVALID, AutofillProfile::VALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER},
      {AutofillProfile::EMPTY, AutofillProfile::VALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID, AutofillProfile::VALID});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID, AutofillProfile::VALID});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLetter, true));
  form_structure->set_password_length_vote(10u);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_passwords_revealed(false);
  upload.set_password_has_letter(true);
  upload.set_password_length(10u);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        nullptr, 3U, {0, 2});
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        nullptr, 5U, {0, 2});
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        nullptr, 9U, {3, 2});
  test::FillUploadField(upload.add_field(), 466116101U, "phone", "number",
                        nullptr, 14U, {1, 2});
  test::FillUploadField(upload.add_field(), 2799270304U, "country",
                        "select-one", nullptr, 36U, {2, 2});

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {PHONE_HOME_WHOLE_NUMBER});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_COUNTRY});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_COUNTRY});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLetter, true));
  form_structure->set_password_length_vote(10u);
  form_structure->set_submission_event(
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_submission_event(AutofillUploadContents::HTML_FORM_SUBMISSION);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_passwords_revealed(false);
  upload.set_password_has_letter(true);
  upload.set_password_length(10u);
  upload.set_action_signature(15724779818122431245U);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        nullptr, 9U);
  test::FillUploadField(upload.add_field(), 466116101U, "phone", "number",
                        nullptr, 14U);
  test::FillUploadField(upload.add_field(), 2799270304U, "country",
                        "select-one", nullptr, 36U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  upload.set_autofill_used(true);
  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.form_control_type = "text";
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});
  }

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLetter, true));
  form_structure->set_password_length_vote(10u);
  form_structure->set_submission_event(
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->host_form_signature =
        form_structure->form_signature();
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Adjust the expected proto string.
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);

  // Create an additional 2 fields (total of 7).
  for (int i = 0; i < 2; ++i) {
    test::FillUploadField(upload.add_field(), 509334676U, "address", "text",
                          nullptr, 30U);
  }
  // Put the appropriate autofill type on the different address fields.
  test::FillUploadField(upload.mutable_field(5), 509334676U, "address", "text",
                        nullptr, 31U);
  test::FillUploadField(upload.mutable_field(6), 509334676U, "address", "text",
                        nullptr, 31U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // Add 300 address fields - now the form is invalid, as it has too many
  // fields.
  for (size_t i = 0; i < 300; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.form_control_type = "text";
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});
  }
  form_structure = std::make_unique<FormStructure>(form);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  EXPECT_TRUE(form_structure
                  ->EncodeUploadRequest(available_field_types, false,
                                        std::string(), true, true)
                  .empty());
}

TEST_F(FormStructureTestImpl,
       EncodeUploadRequestWithAdditionalPasswordFormSignature) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "", "given-name",
                            &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  test::CreateTestFormField("Last Name", "lastname", "", "", "family-name",
                            &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  test::CreateTestFormField("Email", "email", "", "email", "email", &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  test::CreateTestFormField("username", "username", "", "text", &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {USERNAME});

  test::CreateTestFormField("password", "password", "", "password", &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ACCOUNT_CREATION_PASSWORD});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);

    if (form_structure->field(i)->name == u"password") {
      form_structure->field(i)->set_generation_type(
          AutofillUploadContents::Field::
              MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM);
      form_structure->field(i)->set_generated_password_changed(true);
    }
    if (form_structure->field(i)->name == u"username") {
      form_structure->field(i)->set_vote_type(
          AutofillUploadContents::Field::CREDENTIALS_REUSED);
    }
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(USERNAME);
  available_field_types.insert(ACCOUNT_CREATION_PASSWORD);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440000000000000000802");
  upload.set_action_signature(15724779818122431245U);
  upload.set_login_form_signature(42);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  AutofillUploadContents::Field* upload_firstname_field = upload.add_field();
  test::FillUploadField(upload_firstname_field, 4224610201U, "firstname", "",
                        "given-name", 3U);

  AutofillUploadContents::Field* upload_lastname_field = upload.add_field();
  test::FillUploadField(upload_lastname_field, 2786066110U, "lastname", "",
                        "family-name", 5U);

  AutofillUploadContents::Field* upload_email_field = upload.add_field();
  test::FillUploadField(upload_email_field, 1029417091U, "email", "email",
                        "email", 9U);

  AutofillUploadContents::Field* upload_username_field = upload.add_field();
  test::FillUploadField(upload_username_field, 239111655U, "username", "text",
                        "email", 86U);
  upload_username_field->set_vote_type(
      AutofillUploadContents::Field::CREDENTIALS_REUSED);

  AutofillUploadContents::Field* upload_password_field = upload.add_field();
  test::FillUploadField(upload_password_field, 2051817934U, "password",
                        "password", "email", 76U);
  upload_password_field->set_generation_type(
      AutofillUploadContents::Field::
          MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM);
  upload_password_field->set_generated_password_changed(true);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  "42", true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithAutocomplete) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  test::CreateTestFormField("Last Name", "lastname", "", "text", "family-name",
                            &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  test::CreateTestFormField("Email", "email", "", "email", "email", &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        "given-name", 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        "family-name", 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        "email", 9U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequestWithPropertiesMask) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", "given-name",
                            &field);
  field.name_attribute = field.name;
  field.id_attribute = u"first_name";
  field.css_classes = u"class1 class2";
  field.properties_mask = FieldPropertiesFlags::kHadFocus;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  test::CreateTestFormField("Last Name", "lastname", "", "text", "family-name",
                            &field);
  field.name_attribute = field.name;
  field.id_attribute = u"last_name";
  field.css_classes = u"class1 class2";
  field.properties_mask =
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  test::CreateTestFormField("Email", "email", "", "email", "email", &field);
  field.name_attribute = field.name;
  field.id_attribute = u"e-mail";
  field.css_classes = u"class1 class2";
  field.properties_mask =
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, nullptr, nullptr,
                        nullptr, 3U);
  upload.mutable_field(0)->set_properties_mask(FieldPropertiesFlags::kHadFocus);
  test::FillUploadField(upload.add_field(), 3494530716U, nullptr, nullptr,
                        nullptr, 5U);
  upload.mutable_field(1)->set_properties_mask(
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped);
  test::FillUploadField(upload.add_field(), 1029417091U, nullptr, nullptr,
                        nullptr, 9U);
  upload.mutable_field(2)->set_properties_mask(
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped);

  EXPECT_THAT(form_structure->EncodeUploadRequest(
                  available_field_types, true, std::string(), true,
                  /*is_raw_metadata_uploading_enabled=*/false),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_ObservedSubmissionFalse) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = u"firstname";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = u"Last Name";
  field.name = u"lastname";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = u"Email";
  field.name = u"email";
  field.name_attribute = field.name;
  field.form_control_type = "email";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(false);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        nullptr, 9U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(
                  available_field_types, true, std::string(),
                  /* observed_submission= */ false, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithLabels) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  // No label for the first field.
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = u"Last Name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = u"Email";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 9U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithCssClassesAndIds) {
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.css_classes = u"last_name_field";
  field.id_attribute = u"lastname_id";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.css_classes = u"email_field required_field";
  field.id_attribute = u"email_id";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  AutofillUploadContents::Field* firstname_field = upload.add_field();
  test::FillUploadField(firstname_field, 1318412689U, nullptr, "text", nullptr,
                        3U);

  AutofillUploadContents::Field* lastname_field = upload.add_field();
  test::FillUploadField(lastname_field, 1318412689U, nullptr, "text", nullptr,
                        5U);
  lastname_field->set_id("lastname_id");
  lastname_field->set_css_classes("last_name_field");

  AutofillUploadContents::Field* email_field = upload.add_field();
  test::FillUploadField(email_field, 1318412689U, nullptr, "text", nullptr, 9U);
  email_field->set_id("email_id");
  email_field->set_css_classes("email_field required_field");

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

// Test that the form name is sent in the upload request.
TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithFormName) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  // Setting the form name which we expect to see in the upload.
  form.name = u"myform";

  FormFieldData field;
  field.form_control_type = "text";

  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();
  form_structure->set_submission_source(SubmissionSource::FRAME_DETACHED);

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_form_name("myform");
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_FRAME_DETACHED);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 9U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequestPartialMetadata) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  // Some fields don't have "name" or "autocomplete" attributes, and some have
  // neither.
  // No label.
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  test::CreateTestFormField("Last Name", "lastname", "", "text", "family-name",
                            &field);
  field.name_attribute = field.name;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  test::CreateTestFormField("Email", "lastname", "", "email", "email", &field);
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_passwords_revealed(false);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        "family-name", 5U);
  test::FillUploadField(upload.add_field(), 1545468175U, "lastname", "email",
                        "email", 9U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

// Sending field metadata to the server is disabled.
TEST_F(FormStructureTestImpl, EncodeUploadRequest_DisabledMetadata) {
  // Metadata uploading is disabled by a parameter of |EncodeUploadRequest|.
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", "given-name",
                            &field);
  field.name_attribute = field.name;
  field.id_attribute = u"first_name";
  field.css_classes = u"class1 class2";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  test::CreateTestFormField("Last Name", "lastname", "", "text", "family-name",
                            &field);
  field.name_attribute = field.name;
  field.id_attribute = u"last_name";
  field.css_classes = u"class1 class2";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  test::CreateTestFormField("Email", "email", "", "email", "email", &field);
  field.name_attribute = field.name;
  field.id_attribute = u"e-mail";
  field.css_classes = u"class1 class2";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure)
    fs_field->host_form_signature = form_structure->form_signature();

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, nullptr, nullptr,
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, nullptr, nullptr,
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, nullptr, nullptr,
                        nullptr, 9U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(
                  available_field_types, true, std::string(), true,
                  /*is_raw_metadata_uploading_enabled=*/false),
              ElementsSerializeSameAs(upload));
}

// Tests that when the form is the result of flattening multiple forms into one,
// EncodeUploadRequest() returns multiple uploads: one for the entire form and
// one for each of the original forms.
TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithSubForms) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.host_frame = test::MakeLocalFrameToken(test::RandomizeFrame(true));
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"Cardholder name";
  field.name = u"cc-name";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {CREDIT_CARD_NAME_FULL});
  field.host_frame = form.host_frame;
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(123);
  form.fields.push_back(field);

  field.label = u"Credit card number";
  field.name = u"cc-number";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {CREDIT_CARD_NUMBER});
  field.host_frame = test::MakeLocalFrameToken(test::RandomizeFrame(true));
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(456);
  form.fields.push_back(field);

  field.label = u"Expiration date";
  field.name = u"cc-exp";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR});
  field.host_frame = form.host_frame;
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(123);
  form.fields.push_back(field);

  field.label = u"CVC";
  field.name = u"cc-cvc";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {CREDIT_CARD_VERIFICATION_CODE});
  field.host_frame = test::MakeLocalFrameToken(test::RandomizeFrame(true));
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(456);
  form.fields.push_back(field);

  ASSERT_EQ(form.global_id(), form.fields[0].renderer_form_id());
  ASSERT_NE(form.global_id(), form.fields[1].renderer_form_id());
  ASSERT_EQ(form.global_id(), form.fields[2].renderer_form_id());
  ASSERT_NE(form.global_id(), form.fields[3].renderer_form_id());

  form_structure = std::make_unique<FormStructure>(form);

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(CREDIT_CARD_NAME_FULL);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_VERIFICATION_CODE);

  // Prepare the expected proto string.
  const AutofillUploadContents upload_main = [&] {
    AutofillUploadContents upload;
    upload.set_submission(true);
    upload.set_submission_event(
        AutofillUploadContents_SubmissionIndicatorEvent_NONE);
    upload.set_client_version(GetProductNameAndVersionForUserAgent());
    upload.set_form_signature(form_structure->form_signature().value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    upload.set_passwords_revealed(false);
    upload.set_action_signature(15724779818122431245U);
    upload.set_has_form_tag(true);
    test::FillUploadField(upload.add_field(), 3340391946, "cc-name", "text",
                          nullptr, 51);
    test::FillUploadField(upload.add_field(), 1415886167, "cc-number", "text",
                          nullptr, 52);
    test::FillUploadField(upload.add_field(), 3155194603, "cc-exp", "text",
                          nullptr, 57);
    test::FillUploadField(upload.add_field(), 917221285, "cc-cvc", "text",
                          nullptr, 59);
    return upload;
  }();

  const AutofillUploadContents upload_name_exp = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(GetProductNameAndVersionForUserAgent());
    upload.set_form_signature(form.fields[0].host_form_signature.value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field(), 3340391946, "cc-name", "text",
                          nullptr, 51);
    test::FillUploadField(upload.add_field(), 3155194603, "cc-exp", "text",
                          nullptr, 57);
    return upload;
  }();

  const AutofillUploadContents upload_number = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(GetProductNameAndVersionForUserAgent());
    upload.set_form_signature(form.fields[1].host_form_signature.value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field(), 1415886167, "cc-number", "text",
                          nullptr, 52);
    return upload;
  }();

  const AutofillUploadContents upload_cvc = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(GetProductNameAndVersionForUserAgent());
    upload.set_form_signature(form.fields[3].host_form_signature.value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field(), 917221285, "cc-cvc", "text",
                          nullptr, 59);
    return upload;
  }();

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              UnorderedElementsSerializeSameAs(upload_main, upload_name_exp,
                                               upload_number, upload_cvc));
}

// Check that we compute the "datapresent" string correctly for the given
// |available_types|.
TEST_F(FormStructureTestImpl, CheckDataPresence) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"First Name";
  field.name = u"first";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"last";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);
  for (auto& fs_field : form_structure)
    fs_field->host_form_signature = form_structure.form_signature();

  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;

  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities, {UNKNOWN_TYPE});
    form_structure.field(i)->set_possible_types(possible_field_types[i]);
    form_structure.field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // No available types.
  // datapresent should be "" == trimmmed(0x0000000000000000) ==
  //     0b0000000000000000000000000000000000000000000000000000000000000000
  ServerFieldTypeSet available_field_types;

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure.form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("");
  upload.set_passwords_revealed(false);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 1089846351U, "first", "text",
                        nullptr, 1U);
  test::FillUploadField(upload.add_field(), 2404144663U, "last", "text",
                        nullptr, 1U);
  test::FillUploadField(upload.add_field(), 420638584U, "email", "text",
                        nullptr, 1U);

  EXPECT_THAT(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // Only a few types available.
  // datapresent should be "1540000240" == trimmmed(0x1540000240000000) ==
  //     0b0001010101000000000000000000001001000000000000000000000000000000
  // The set bits are:
  //  3 == NAME_FIRST
  //  5 == NAME_LAST
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 30 == ADDRESS_HOME_LINE1
  // 33 == ADDRESS_HOME_CITY
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_CITY);

  // Adjust the expected proto string.
  upload.set_data_present("1540000240");
  EXPECT_THAT(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // All supported non-credit card types available.
  // datapresent should be "1f7e000378000008" == trimmmed(0x1f7e000378000008) ==
  //     0b0001111101111110000000000000001101111000000000000000000000001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  4 == NAME_MIDDLE
  //  5 == NAME_LAST
  //  6 == NAME_MIDDLE_INITIAL
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 10 == PHONE_HOME_NUMBER,
  // 11 == PHONE_HOME_CITY_CODE,
  // 12 == PHONE_HOME_COUNTRY_CODE,
  // 13 == PHONE_HOME_CITY_AND_NUMBER,
  // 14 == PHONE_HOME_WHOLE_NUMBER,
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 35 == ADDRESS_HOME_ZIP
  // 36 == ADDRESS_HOME_COUNTRY
  // 60 == COMPANY_NAME
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_MIDDLE);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_MIDDLE_INITIAL);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_NUMBER);
  available_field_types.insert(PHONE_HOME_CITY_CODE);
  available_field_types.insert(PHONE_HOME_COUNTRY_CODE);
  available_field_types.insert(PHONE_HOME_CITY_AND_NUMBER);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(ADDRESS_HOME_ZIP);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(COMPANY_NAME);

  // Adjust the expected proto string.
  upload.set_data_present("1f7e000378000008");
  EXPECT_THAT(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // All supported credit card types available.
  // datapresent should be "0000000000001fc0" == trimmmed(0x0000000000001fc0) ==
  //     0b0000000000000000000000000000000000000000000000000001111111000000
  // The set bits are:
  // 51 == CREDIT_CARD_NAME_FULL
  // 52 == CREDIT_CARD_NUMBER
  // 53 == CREDIT_CARD_EXP_MONTH
  // 54 == CREDIT_CARD_EXP_2_DIGIT_YEAR
  // 55 == CREDIT_CARD_EXP_4_DIGIT_YEAR
  // 56 == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  // 57 == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  available_field_types.clear();
  available_field_types.insert(CREDIT_CARD_NAME_FULL);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_MONTH);
  available_field_types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);

  // Adjust the expected proto string.
  upload.set_data_present("0000000000001fc0");
  EXPECT_THAT(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // All supported types available.
  // datapresent should be "1f7e000378001fc8" == trimmmed(0x1f7e000378001fc8) ==
  //     0b0001111101111110000000000000001101111000000000000001111111001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  4 == NAME_MIDDLE
  //  5 == NAME_LAST
  //  6 == NAME_MIDDLE_INITIAL
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 10 == PHONE_HOME_NUMBER,
  // 11 == PHONE_HOME_CITY_CODE,
  // 12 == PHONE_HOME_COUNTRY_CODE,
  // 13 == PHONE_HOME_CITY_AND_NUMBER,
  // 14 == PHONE_HOME_WHOLE_NUMBER,
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 35 == ADDRESS_HOME_ZIP
  // 36 == ADDRESS_HOME_COUNTRY
  // 51 == CREDIT_CARD_NAME_FULL
  // 52 == CREDIT_CARD_NUMBER
  // 53 == CREDIT_CARD_EXP_MONTH
  // 54 == CREDIT_CARD_EXP_2_DIGIT_YEAR
  // 55 == CREDIT_CARD_EXP_4_DIGIT_YEAR
  // 56 == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  // 57 == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  // 60 == COMPANY_NAME
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_MIDDLE);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_MIDDLE_INITIAL);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_NUMBER);
  available_field_types.insert(PHONE_HOME_CITY_CODE);
  available_field_types.insert(PHONE_HOME_COUNTRY_CODE);
  available_field_types.insert(PHONE_HOME_CITY_AND_NUMBER);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(ADDRESS_HOME_ZIP);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(CREDIT_CARD_NAME_FULL);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_MONTH);
  available_field_types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  available_field_types.insert(COMPANY_NAME);

  // Adjust the expected proto string.
  upload.set_data_present("1f7e000378001fc8");
  EXPECT_THAT(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, CheckMultipleTypes) {
  // Throughout this test, datapresent should be
  // 0x1440000360000008 ==
  //     0b0001010001000000000000000000001101100000000000000000000000001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  5 == NAME_LAST
  //  9 == EMAIL_ADDRESS
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 60 == COMPANY_NAME
  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(COMPANY_NAME);

  // Check that multiple types for the field are processed correctly.
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = false;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"email";
  field.name = u"email";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  field.label = u"First Name";
  field.name = u"first";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = u"Last Name";
  field.name = u"last";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = u"Address";
  field.name = u"address";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_LINE1});

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_source(SubmissionSource::XHR_SUCCEEDED);
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->host_form_signature =
        form_structure->form_signature();
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1440000360000008");
  upload.set_passwords_revealed(false);
  upload.set_has_form_tag(false);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_XHR_SUCCEEDED);

  test::FillUploadField(upload.add_field(), 420638584U, "email", "text",
                        nullptr, 9U);
  test::FillUploadField(upload.add_field(), 1089846351U, "first", "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 2404144663U, "last", "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 509334676U, "address", "text",
                        nullptr, 30U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // Match third field as both first and last.
  possible_field_types[2].insert(NAME_FIRST);
  form_structure->field(2)->set_possible_types(possible_field_types[2]);

  // Modify the expected upload.
  // Add the NAME_FIRST prediction to the third field.
  test::FillUploadField(upload.mutable_field(2), 2404144663U, "last", "text",
                        nullptr, 3U);

  upload.mutable_field(2)->mutable_autofill_type()->SwapElements(0, 1);
  upload.mutable_field(2)->mutable_autofill_type_validities()->SwapElements(0,
                                                                            1);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // Match last field as both address home line 1 and 2.
  possible_field_types[3].insert(ADDRESS_HOME_LINE2);
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types(
          possible_field_types[form_structure->field_count() - 1]);

  // Adjust the expected upload proto.
  test::FillUploadField(upload.mutable_field(3), 509334676U, "address", "text",
                        nullptr, 31U);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));

  // Replace the address line 2 prediction by company name.
  possible_field_types[3].clear();
  possible_field_types[3].insert(ADDRESS_HOME_LINE1);
  possible_field_types[3].insert(COMPANY_NAME);
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types(
          possible_field_types[form_structure->field_count() - 1]);
  possible_field_types_validities[3].clear();
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types_validities(
          possible_field_types_validities[form_structure->field_count() - 1]);

  // Adjust the expected upload proto.
  upload.mutable_field(3)->mutable_autofill_type_validities(1)->set_type(60);
  upload.mutable_field(3)->set_autofill_type(1, 60);

  EXPECT_THAT(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), true, true),
              ElementsSerializeSameAs(upload));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_PasswordsRevealed) {
  FormData form;
  form.url = GURL("http://www.foo.com/");

  // Add 3 fields, to make the form uploadable.
  FormFieldData field;
  field.name = u"email";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.name = u"first";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.name = u"last";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_passwords_were_revealed(true);
  for (auto& fs_field : form_structure)
    fs_field->host_form_signature = form_structure.form_signature();

  std::vector<AutofillUploadContents> uploads =
      form_structure.EncodeUploadRequest(
          {{}} /* available_field_types */, false /* form_was_autofilled */,
          std::string() /* login_form_signature */,
          true /* observed_submission */,
          true /* is_raw_metadata_uploading_enabled */);
  ASSERT_EQ(1u, uploads.size());
  EXPECT_EQ(true, uploads.front().passwords_revealed());
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_IsFormTag) {
  for (bool is_form_tag : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_form_tag=" << is_form_tag);

    FormData form;
    form.url = GURL("http://www.foo.com/");
    FormFieldData field;
    field.name = u"email";
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);

    form.is_form_tag = is_form_tag;

    FormStructure form_structure(form);
    for (auto& fs_field : form_structure)
      fs_field->host_form_signature = form_structure.form_signature();
    form_structure.set_passwords_were_revealed(true);
    std::vector<AutofillUploadContents> uploads =
        form_structure.EncodeUploadRequest(
            {{}} /* available_field_types */, false /* form_was_autofilled */,
            std::string() /* login_form_signature */,
            true /* observed_submission */,
            false /* is_raw_metadata_uploading_enabled */);
    ASSERT_EQ(1u, uploads.size());
    EXPECT_EQ(is_form_tag, uploads.front().has_form_tag());
  }
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_RichMetadata) {
  struct FieldMetadata {
    const char *id, *name, *label, *placeholder, *aria_label, *aria_description,
        *css_classes, *autocomplete;
  };

  static const FieldMetadata kFieldMetadata[] = {
      {"fname_id", "fname_name", "First Name:", "Please enter your first name",
       "Type your first name", "You can type your first name here", "blah",
       "given-name"},
      {"lname_id", "lname_name", "Last Name:", "Please enter your last name",
       "Type your lat name", "You can type your last name here", "blah",
       "family-name"},
      {"email_id", "email_name", "Email:", "Please enter your email address",
       "Type your email address", "You can type your email address here",
       "blah", "email"},
      {"id_only", "", "", "", "", "", "", ""},
      {"", "name_only", "", "", "", "", "", ""},
  };

  FormData form;
  form.id_attribute = u"form-id";
  form.url = GURL("http://www.foo.com/");
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.full_url = GURL("http://www.foo.com/?foo=bar");
  for (const auto& f : kFieldMetadata) {
    FormFieldData field;
    field.id_attribute = ASCIIToUTF16(f.id);
    field.name_attribute = ASCIIToUTF16(f.name);
    field.name = field.name_attribute;
    field.label = ASCIIToUTF16(f.label);
    field.placeholder = ASCIIToUTF16(f.placeholder);
    field.aria_label = ASCIIToUTF16(f.aria_label);
    field.aria_description = ASCIIToUTF16(f.aria_description);
    field.css_classes = ASCIIToUTF16(f.css_classes);
    field.autocomplete_attribute = f.autocomplete;
    field.parsed_autocomplete =
        ParseAutocompleteAttribute(f.autocomplete, /*field_max_length=*/0);
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
  }
  RandomizedEncoder encoder("seed for testing",
                            AutofillRandomizedValue_EncodingType_ALL_BITS,
                            /*anonymous_url_collection_is_enabled*/ true);

  FormStructure form_structure(form);
  form_structure.set_randomized_encoder(
      std::make_unique<RandomizedEncoder>(encoder));
  for (auto& field : form_structure)
    field->host_form_signature = form_structure.form_signature();

  std::vector<AutofillUploadContents> uploads =
      form_structure.EncodeUploadRequest(
          {{}} /* available_field_types */, false /* form_was_autofilled */,
          std::string() /* login_form_signature */,
          true /* observed_submission */,
          false /* is_raw_metadata_uploading_enabled */);
  ASSERT_EQ(1u, uploads.size());
  AutofillUploadContents& upload = uploads.front();

  const auto form_signature = form_structure.form_signature();

  if (form.id_attribute.empty()) {
    EXPECT_FALSE(upload.randomized_form_metadata().has_id());
  } else {
    EXPECT_EQ(upload.randomized_form_metadata().id().encoded_bits(),
              encoder.EncodeForTesting(form_signature, FieldSignature(),
                                       RandomizedEncoder::FORM_ID,
                                       form_structure.id_attribute()));
  }

  if (form.name_attribute.empty()) {
    EXPECT_FALSE(upload.randomized_form_metadata().has_name());
  } else {
    EXPECT_EQ(upload.randomized_form_metadata().name().encoded_bits(),
              encoder.EncodeForTesting(form_signature, FieldSignature(),
                                       RandomizedEncoder::FORM_NAME,
                                       form_structure.name_attribute()));
  }

  auto full_url = form_structure.full_source_url().spec();
  EXPECT_EQ(upload.randomized_form_metadata().url().encoded_bits(),
            encoder.Encode(form_signature, FieldSignature(),
                           RandomizedEncoder::FORM_URL, full_url));
  ASSERT_EQ(static_cast<size_t>(upload.field_size()),
            std::size(kFieldMetadata));

  ASSERT_EQ(1, upload.randomized_form_metadata().button_title().size());
  EXPECT_EQ(upload.randomized_form_metadata()
                .button_title()[0]
                .title()
                .encoded_bits(),
            encoder.EncodeForTesting(form_signature, FieldSignature(),
                                     RandomizedEncoder::FORM_BUTTON_TITLES,
                                     form.button_titles[0].first));
  EXPECT_EQ(ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE,
            upload.randomized_form_metadata().button_title()[0].type());

  for (int i = 0; i < upload.field_size(); ++i) {
    const auto& metadata = upload.field(i).randomized_field_metadata();
    const auto& field = *form_structure.field(i);
    const auto field_signature = field.GetFieldSignature();
    if (field.id_attribute.empty()) {
      EXPECT_FALSE(metadata.has_id());
    } else {
      EXPECT_EQ(metadata.id().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_ID,
                                         field.id_attribute));
    }
    if (field.name.empty()) {
      EXPECT_FALSE(metadata.has_name());
    } else {
      EXPECT_EQ(metadata.name().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_NAME,
                                         field.name_attribute));
    }
    if (field.form_control_type.empty()) {
      EXPECT_FALSE(metadata.has_type());
    } else {
      EXPECT_EQ(metadata.type().encoded_bits(),
                encoder.Encode(form_signature, field_signature,
                               RandomizedEncoder::FIELD_CONTROL_TYPE,
                               field.form_control_type));
    }
    if (field.label.empty()) {
      EXPECT_FALSE(metadata.has_label());
    } else {
      EXPECT_EQ(metadata.label().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_LABEL,
                                         field.label));
    }
    if (field.aria_label.empty()) {
      EXPECT_FALSE(metadata.has_aria_label());
    } else {
      EXPECT_EQ(metadata.aria_label().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_ARIA_LABEL,
                                         field.aria_label));
    }
    if (field.aria_description.empty()) {
      EXPECT_FALSE(metadata.has_aria_description());
    } else {
      EXPECT_EQ(
          metadata.aria_description().encoded_bits(),
          encoder.EncodeForTesting(form_signature, field_signature,
                                   RandomizedEncoder::FIELD_ARIA_DESCRIPTION,
                                   field.aria_description));
    }
    if (field.css_classes.empty()) {
      EXPECT_FALSE(metadata.has_css_class());
    } else {
      EXPECT_EQ(metadata.css_class().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_CSS_CLASS,
                                         field.css_classes));
    }
    if (field.placeholder.empty()) {
      EXPECT_FALSE(metadata.has_placeholder());
    } else {
      EXPECT_EQ(metadata.placeholder().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_PLACEHOLDER,
                                         field.placeholder));
    }
    if (field.autocomplete_attribute.empty()) {
      EXPECT_FALSE(metadata.has_autocomplete());
    } else {
      EXPECT_EQ(metadata.autocomplete().encoded_bits(),
                encoder.EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::FIELD_AUTOCOMPLETE,
                    base::UTF8ToUTF16(field.autocomplete_attribute)));
    }
  }
}

TEST_F(FormStructureTestImpl, Metadata_OnlySendFullUrlWithUserConsent) {
  for (bool has_consent : {true, false}) {
    SCOPED_TRACE(testing::Message() << " has_consent=" << has_consent);
    FormData form;
    form.id_attribute = u"form-id";
    form.url = GURL("http://www.foo.com/");
    form.full_url = GURL("http://www.foo.com/?foo=bar");

    // One form field needed to be valid form.
    FormFieldData field;
    field.form_control_type = "text";
    field.label = u"email";
    field.name = u"email";
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);

    TestingPrefServiceSimple prefs;
    prefs.registry()->RegisterBooleanPref(
        RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled, false);
    prefs.SetBoolean(
        RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled,
        has_consent);
    prefs.registry()->RegisterStringPref(prefs::kAutofillUploadEncodingSeed,
                                         "default_secret");
    prefs.SetString(prefs::kAutofillUploadEncodingSeed, "user_secret");

    FormStructure form_structure(form);
    form_structure.set_randomized_encoder(RandomizedEncoder::Create(&prefs));
    std::vector<AutofillUploadContents> uploads =
        form_structure.EncodeUploadRequest({}, true, "", true, true);

    EXPECT_EQ(has_consent,
              uploads.front().randomized_form_metadata().has_url());
  }
}

TEST_F(FormStructureTestImpl, CheckFormSignature) {
  // Check that form signature is created correctly.
  std::unique_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"email";
  field.name = u"email";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"First Name";
  field.name = u"first";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Checkable fields shouldn't affect the signature.
  field.label = u"Select";
  field.name = u"Select";
  field.form_control_type = "checkbox";
  field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  field.unique_renderer_id = test::MakeFieldRendererId();
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
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Random Field label2";
  field.name = u"random12345";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Random Field label3";
  field.name = u"1ran12dom12345678";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Random Field label3";
  field.name = u"12345ran123456dom123";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("https://login.facebook.com&login_form&email&first&"
                            "random1234&random&1ran12dom&random123")),
            form_structure->FormSignatureAsStr());
}

TEST_F(FormStructureTestImpl, ToFormData) {
  FormData form;
  form.name = u"the-name";
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  field.label = u"username";
  field.name = u"username";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"password";
  field.name = u"password";
  field.form_control_type = "password";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"Submit";
  field.form_control_type = "submit";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_TRUE(form.SameFormAs(FormStructure(form).ToFormData()));
}

TEST_F(FormStructureTestImpl, SkipFieldTest) {
  FormData form;
  form.name = u"the-name";
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  field.label = u"username";
  field.name = u"username";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"select";
  field.name = u"select";
  field.form_control_type = "checkbox";
  field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  field.name = u"email";
  field.form_control_type = "text";
  field.check_status = FormFieldData::CheckStatus::kNotCheckable;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(GetProductNameAndVersionForUserAgent());
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(420638584U);

  const FormSignature kExpectedSignature(18006745212084723782UL);

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures.front());
  EXPECT_THAT(encoded_query, SerializesSameAs(query));
}

TEST_F(FormStructureTestImpl, EncodeQueryRequest_WithLabels) {
  FormData form;
  form.name = u"the-name";
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  // No label on the first field.
  field.name = u"username";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Enter your Email address";
  field.name = u"email";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Enter your Password";
  field.name = u"password";
  field.form_control_type = "password";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  std::vector<FormStructure*> forms;
  FormStructure form_structure(form);
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(GetProductNameAndVersionForUserAgent());
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(420638584U);
  query_form->add_fields()->set_signature(2051817934U);

  EXPECT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  EXPECT_THAT(encoded_query, SerializesSameAs(query));
}

TEST_F(FormStructureTestImpl, EncodeQueryRequest_WithLongLabels) {
  FormData form;
  form.name = u"the-name";
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  // No label on the first field.
  field.name = u"username";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // This label will be truncated in the XML request.
  field.label =
      u"Enter Your Really Really Really (Really!) Long Email Address Which We "
      u"Hope To Get In Order To Send You Unwanted Publicity Because That's "
      u"What Marketers Do! We Know That Your Email Address Has The Possibility "
      u"Of Exceeding A Certain Number Of Characters...";
  field.name = u"email";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Enter your Password";
  field.name = u"password";
  field.form_control_type = "password";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(GetProductNameAndVersionForUserAgent());
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(420638584U);
  query_form->add_fields()->set_signature(2051817934U);

  EXPECT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  EXPECT_THAT(encoded_query, SerializesSameAs(query));
}

// One name is missing from one field.
TEST_F(FormStructureTestImpl, EncodeQueryRequest_MissingNames) {
  FormData form;
  // No name set for the form.
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  field.label = u"username";
  field.name = u"username";
  field.form_control_type = "text";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = std::u16string();
  // No name set for this field.
  field.name = u"";
  field.form_control_type = "text";
  field.check_status = FormFieldData::CheckStatus::kNotCheckable;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  for (auto& fs_field : form_structure)
    fs_field->host_form_signature = form_structure.form_signature();

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(GetProductNameAndVersionForUserAgent());
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(1318412689U);

  const FormSignature kExpectedSignature(16416961345885087496UL);

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures.front());
  EXPECT_THAT(encoded_query, SerializesSameAs(query));
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithSingleUsernameVoteType) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  FormFieldData field;
  field.name = u"text field";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.field(0)->set_single_username_vote_type(
      AutofillUploadContents::Field::STRONG);
  for (auto& fs_field : form_structure)
    fs_field->host_form_signature = form_structure.form_signature();

  std::vector<AutofillUploadContents> uploads =
      form_structure.EncodeUploadRequest(
          {{}} /* available_field_types */, false /* form_was_autofilled */,
          std::string() /* login_form_signature */,
          true /* observed_submission */,
          false /* is_raw_metadata_uploading_enabled */);
  ASSERT_EQ(1u, uploads.size());
  EXPECT_EQ(form_structure.field(0)->single_username_vote_type(),
            uploads.front().field(0).single_username_vote_type());
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithSingleUsernameData) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  FormFieldData field_data;
  field_data.name = u"text field";
  field_data.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field_data);

  FormStructure form_structure(form);
  for (auto& field : form_structure)
    field->host_form_signature = form_structure.form_signature();

  AutofillUploadContents::SingleUsernameData single_username_data;
  single_username_data.set_username_form_signature(12345);
  single_username_data.set_username_field_signature(678910);
  single_username_data.set_value_type(AutofillUploadContents::EMAIL);
  single_username_data.set_prompt_edit(AutofillUploadContents::EDITED_POSITIVE);
  form_structure.set_single_username_data(single_username_data);

  std::vector<AutofillUploadContents> uploads =
      form_structure.EncodeUploadRequest(
          {{}} /* available_field_types */, false /* form_was_autofilled */,
          std::string() /* login_form_signature */,
          true /* observed_submission */,
          false /* is_raw_metadata_uploading_enabled */);
  ASSERT_EQ(1u, uploads.size());
  EXPECT_EQ(form_structure.single_username_data()->username_form_signature(),
            uploads.front().single_username_data().username_form_signature());
  EXPECT_EQ(form_structure.single_username_data()->username_field_signature(),
            uploads.front().single_username_data().username_field_signature());
  EXPECT_EQ(form_structure.single_username_data()->value_type(),
            uploads.front().single_username_data().value_type());
  EXPECT_EQ(form_structure.single_username_data()->prompt_edit(),
            uploads.front().single_username_data().prompt_edit());
}

// Test that server overrides get precedence over HTML types.
TEST_F(FormStructureTestImpl, ParseQueryResponse_ServerPredictionIsOverride) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");
  field.form_control_type = "text";

  // Just some field with an autocomplete attribute.
  test::CreateTestFormField("some field", "some_field", "", "text", "name",
                            &field);
  form_data.fields.push_back(field);

  // Some other field with the same autocomplete attribute.
  test::CreateTestFormField("some other field", "some_other_field", "", "text",
                            "name", &field);
  form_data.fields.push_back(field);

  // Setup the query response with an override for the name field to be a first
  // name.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldOverrideToForm(form_data.fields[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[1], NAME_LAST, form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);
  ASSERT_EQ(form.field_count(), 2U);

  // Validate the type predictions.
  EXPECT_EQ(UNKNOWN_TYPE, form.field(0)->heuristic_type());
  EXPECT_EQ(HtmlFieldType::kName, form.field(0)->html_type());
  EXPECT_EQ(NAME_FIRST, form.field(0)->server_type());
  EXPECT_EQ(UNKNOWN_TYPE, form.field(1)->heuristic_type());
  EXPECT_EQ(HtmlFieldType::kName, form.field(1)->html_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());

  // Validate that the overrides are set correctly.
  EXPECT_TRUE(form.field(0)->server_type_prediction_is_override());
  EXPECT_FALSE(form.field(1)->server_type_prediction_is_override());

  // Validate that the server prediction won for the first field.
  EXPECT_EQ(form.field(0)->Type().GetStorableType(), NAME_FIRST);
  EXPECT_EQ(form.field(1)->Type().GetStorableType(), NAME_FULL);

  // Validate that the server override cannot be altered.
  form.field(0)->SetTypeTo(AutofillType(NAME_FULL));
  EXPECT_EQ(form.field(0)->Type().GetStorableType(), NAME_FIRST);

  // Validate that that the non-override can be altered.
  form.field(1)->SetTypeTo(AutofillType(NAME_FIRST));
  EXPECT_EQ(form.field(1)->Type().GetStorableType(), NAME_FIRST);
}

// Test the heuristic prediction for NAME_LAST_SECOND overrides server
// predictions.
TEST_F(FormStructureTestImpl,
       ParseQueryResponse_HeuristicsOverrideSpanishLastNameTypes) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");
  field.form_control_type = "text";

  // First name field.
  field.label = u"Nombre";
  field.name = u"Nombre";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  // First last name field.
  // Should be identified by local heuristics.
  field.label = u"Apellido Paterno";
  field.name = u"apellido_paterno";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  // Second last name field.
  // Should be identified by local heuristics.
  field.label = u"Apellido Materno";
  field.name = u"apellido materno";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields[0], NAME_FIRST, form_suggestion);
  // Simulate a NAME_LAST classification for the two last name fields.
  AddFieldPredictionToForm(form_data.fields[1], NAME_LAST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[2], NAME_LAST, form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate the heuristic and server predictions.
  EXPECT_EQ(NAME_LAST_FIRST, form.field(1)->heuristic_type());
  EXPECT_EQ(NAME_LAST_SECOND, form.field(2)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());
  EXPECT_EQ(NAME_LAST, form.field(2)->server_type());

  // Validate that the heuristic prediction wins for the two last name fields.
  EXPECT_EQ(form.field(0)->Type().GetStorableType(), NAME_FIRST);
  EXPECT_EQ(form.field(1)->Type().GetStorableType(), NAME_LAST_FIRST);
  EXPECT_EQ(form.field(2)->Type().GetStorableType(), NAME_LAST_SECOND);
}

// Test the heuristic prediction for ADDRESS_HOME_STREET_NAME and
// ADDRESS_HOME_HOUSE_NUMBER overrides server predictions.
TEST_F(FormStructureTestImpl,
       ParseQueryResponse_HeuristicsOverrideStreetNameAndHouseNumberTypes) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");
  field.form_control_type = "text";

  // Field for the name.
  field.label = u"Name";
  field.name = u"Name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  // Field for the street name.
  field.label = u"Street Name";
  field.name = u"street_name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  // Field for the house number.
  field.label = u"House Number";
  field.name = u"house_number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  // Field for the postal code.
  field.label = u"ZIP";
  field.name = u"ZIP";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields[0], NAME_FULL, form_suggestion);
  // Simulate ADDRESS_LINE classifications for the two last name fields.
  AddFieldPredictionToForm(form_data.fields[1], ADDRESS_HOME_LINE1,
                           form_suggestion);
  AddFieldPredictionToForm(form_data.fields[2], ADDRESS_HOME_LINE2,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);
  ASSERT_EQ(form.field_count(), 4U);

  // Validate the heuristic and server predictions.
  EXPECT_EQ(ADDRESS_HOME_STREET_NAME, form.field(1)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_HOUSE_NUMBER, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(1)->server_type());
  EXPECT_EQ(ADDRESS_HOME_LINE2, form.field(2)->server_type());

  // Validate that the heuristic prediction wins for the street name and house
  // number.
  EXPECT_EQ(form.field(1)->Type().GetStorableType(), ADDRESS_HOME_STREET_NAME);
  EXPECT_EQ(form.field(2)->Type().GetStorableType(), ADDRESS_HOME_HOUSE_NUMBER);
}

// Tests proper resolution heuristic, server and html field types when the
// server returns NO_SERVER_DATA, UNKNOWN_TYPE, and a valid type.
TEST_F(FormStructureTestImpl, ParseQueryResponse_TooManyTypes) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");

  test::CreateTestFormField("First Name", "fname", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lname", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("email", "email", "", "text", "address-level2",
                            &field);
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[1], NAME_LAST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[2], ADDRESS_HOME_LINE1,
                           form_suggestion);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(
      EMAIL_ADDRESS);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(
      UNKNOWN_TYPE);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate field 0.
  EXPECT_EQ(NAME_FIRST, form.field(0)->heuristic_type());
  EXPECT_EQ(NAME_FIRST, form.field(0)->server_type());
  EXPECT_EQ(HtmlFieldType::kUnspecified, form.field(0)->html_type());
  EXPECT_EQ(NAME_FIRST, form.field(0)->Type().GetStorableType());

  // Validate field 1.
  EXPECT_EQ(NAME_LAST, form.field(1)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());
  EXPECT_EQ(HtmlFieldType::kUnspecified, form.field(1)->html_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->Type().GetStorableType());

  // Validate field 2. Note: HtmlFieldType::kAddressLevel2 -> City
  EXPECT_EQ(EMAIL_ADDRESS, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(2)->server_type());
  EXPECT_EQ(HtmlFieldType::kAddressLevel2, form.field(2)->html_type());
  EXPECT_EQ(ADDRESS_HOME_CITY, form.field(2)->Type().GetStorableType());

  // Also check the extreme case of an empty form.
  FormStructure empty_form{FormData()};
  std::vector<FormStructure*> empty_forms{&empty_form};
  FormStructure::ParseApiQueryResponse(response_string, empty_forms,
                                       test::GetEncodedSignatures(empty_forms),
                                       nullptr, nullptr);
  ASSERT_EQ(empty_form.field_count(), 0U);
}

// Tests proper resolution heuristic, server and html field types when the
// server returns NO_SERVER_DATA, UNKNOWN_TYPE, and a valid type.
TEST_F(FormStructureTestImpl, ParseQueryResponse_UnknownType) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");

  test::CreateTestFormField("First Name", "fname", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lname", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("email", "email", "", "text", "address-level2",
                            &field);
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields[0], UNKNOWN_TYPE, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[1], NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(form_data.fields[2], ADDRESS_HOME_LINE1,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate field 0.
  EXPECT_EQ(NAME_FIRST, form.field(0)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form.field(0)->server_type());
  EXPECT_EQ(HtmlFieldType::kUnspecified, form.field(0)->html_type());
  EXPECT_EQ(UNKNOWN_TYPE, form.field(0)->Type().GetStorableType());

  // Validate field 1.
  EXPECT_EQ(NAME_LAST, form.field(1)->heuristic_type());
  EXPECT_EQ(NO_SERVER_DATA, form.field(1)->server_type());
  EXPECT_EQ(HtmlFieldType::kUnspecified, form.field(1)->html_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->Type().GetStorableType());

  // Validate field 2. Note: HtmlFieldType::kAddressLevel2 -> City
  EXPECT_EQ(EMAIL_ADDRESS, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(2)->server_type());
  EXPECT_EQ(HtmlFieldType::kAddressLevel2, form.field(2)->html_type());
  EXPECT_EQ(ADDRESS_HOME_CITY, form.field(2)->Type().GetStorableType());
}

// Tests that the signatures of a field's FormFieldData::host_form_signature are
// used as a fallback if the form's signature does not contain useful type
// predictions.
TEST_F(FormStructureTestImpl, ParseApiQueryResponseWithDifferentRendererForms) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kAutofillAcrossIframes);

  std::vector<ServerFieldType> expected_types;

  // Create a form whose fields have FormFieldData::host_form_signature either
  // 12345 or 67890. The first two fields have identical field signatures.
  std::vector<FormFieldData> fields;
  FormFieldData field;
  field.form_control_type = "text";

  field.name = u"name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(12345);
  fields.push_back(field);
  expected_types.push_back(CREDIT_CARD_NAME_FIRST);

  field.name = u"name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(12345);
  fields.push_back(field);
  expected_types.push_back(CREDIT_CARD_NAME_LAST);

  field.name = u"number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(12345);
  fields.push_back(field);
  expected_types.push_back(CREDIT_CARD_NUMBER);

  field.name = u"exp_month";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(67890);
  fields.push_back(field);
  expected_types.push_back(CREDIT_CARD_EXP_MONTH);

  field.name = u"exp_year";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(67890);
  fields.push_back(field);
  expected_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);

  field.name = u"cvc";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(67890);
  fields.push_back(field);
  expected_types.push_back(CREDIT_CARD_VERIFICATION_CODE);

  field.name = u"";
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(67890);
  fields.push_back(field);
  expected_types.push_back(NO_SERVER_DATA);

  FormData form;
  form.fields = fields;
  form.url = GURL("http://foo.com");

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  ASSERT_GE(fields.size(), 6u);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  // Response for the form's signature:
  // - The predictions for `fields[1]`, `fields[2]`, `fields[5]` are expected to
  //   be overridden by the FormFieldData::host_form_signature predictions.
  // - Since fields 0 and 1 have identical signatures, the client must consider
  //   the fields' rank in FormData::host_form_signature's predictions
  //   to obtain the right prediction for `fields[1]`.
  // - `fields[6]` has no predictions at all.
  std::vector<FormSignature> encoded_signatures =
      test::GetEncodedSignatures(forms);
  {
    auto* form_suggestion = api_response.add_form_suggestions();
    AddFieldPredictionToForm(fields[0], expected_types[0], form_suggestion);
    AddFieldPredictionToForm(fields[1], NO_SERVER_DATA, form_suggestion);
    AddFieldPredictionToForm(fields[2], NO_SERVER_DATA, form_suggestion);
    AddFieldPredictionToForm(fields[3], expected_types[3], form_suggestion);
    AddFieldPredictionToForm(fields[4], expected_types[4], form_suggestion);
  }
  // Response for the FormFieldData::host_form_signature 12345.
  encoded_signatures.push_back(FormSignature(12345));
  {
    auto* form_suggestion = api_response.add_form_suggestions();
    AddFieldPredictionToForm(fields[0], NO_SERVER_DATA, form_suggestion);
    AddFieldPredictionToForm(fields[1], expected_types[1], form_suggestion);
    AddFieldPredictionToForm(fields[2], expected_types[2], form_suggestion);
  }
  // Response for the FormFieldData::host_form_signature 67890.
  encoded_signatures.push_back(FormSignature(67890));
  {
    auto* form_suggestion = api_response.add_form_suggestions();
    AddFieldPredictionToForm(fields[4], ADDRESS_HOME_CITY, form_suggestion);
    AddFieldPredictionToForm(fields[5], expected_types[5], form_suggestion);
  }

  // Serialize API response.
  std::string response_string;
  std::string encoded_response_string;
  ASSERT_TRUE(api_response.SerializeToString(&response_string));
  base::Base64Encode(response_string, &encoded_response_string);

  FormStructure::ParseApiQueryResponse(std::move(encoded_response_string),
                                       forms, encoded_signatures, nullptr,
                                       nullptr);

  // Check expected field types.
  ASSERT_GE(forms[0]->field_count(), 6U);
  ASSERT_EQ(forms[0]->field(0)->GetFieldSignature(),
            forms[0]->field(1)->GetFieldSignature());
  EXPECT_EQ(forms.front()->field(0)->server_type(), expected_types[0]);
  EXPECT_EQ(forms.front()->field(1)->server_type(), expected_types[1]);
  EXPECT_EQ(forms.front()->field(2)->server_type(), expected_types[2]);
  EXPECT_EQ(forms.front()->field(3)->server_type(), expected_types[3]);
  EXPECT_EQ(forms.front()->field(4)->server_type(), expected_types[4]);
  EXPECT_EQ(forms.front()->field(5)->server_type(), expected_types[5]);
  EXPECT_EQ(forms.front()->field(6)->server_type(), expected_types[6]);
}

TEST_F(FormStructureTestImpl, ParseApiQueryResponse) {
  // Make form 1 data.
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";

  field.label = u"fullname";
  field.name = u"fullname";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"address";
  field.name = u"address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Checkable fields should be ignored in parsing
  FormFieldData checkable_field;
  checkable_field.label = u"radio_button";
  checkable_field.form_control_type = "radio";
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Make form 2 data.
  FormData form2;
  field.label = u"email";
  field.name = u"email";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form2.fields.push_back(field);

  field.label = u"password";
  field.name = u"password";
  field.form_control_type = "password";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.push_back(&form_structure2);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  // Make form 1 suggestions.
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(form.fields[0],
                            {CreateFieldPrediction(NAME_FULL),
                             CreateFieldPrediction(PHONE_HOME_COUNTRY_CODE)},
                            form_suggestion);
  AddFieldPredictionToForm(form.fields[1], ADDRESS_HOME_LINE1, form_suggestion);
  // Make form 2 suggestions.
  form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionToForm(form2.fields[0], EMAIL_ADDRESS, form_suggestion);
  AddFieldPredictionToForm(form2.fields[1], NO_SERVER_DATA, form_suggestion);
  // Serialize API response.
  std::string response_string;
  std::string encoded_response_string;
  ASSERT_TRUE(api_response.SerializeToString(&response_string));
  base::Base64Encode(response_string, &encoded_response_string);

  FormStructure::ParseApiQueryResponse(std::move(encoded_response_string),
                                       forms, test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  // Verify that the form fields are properly filled with data retrieved from
  // the query.
  ASSERT_GE(forms[0]->field_count(), 2U);
  ASSERT_GE(forms[1]->field_count(), 2U);

  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(NAME_FULL),
                          EqualsPrediction(PHONE_HOME_COUNTRY_CODE)));

  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->server_type());
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(ADDRESS_HOME_LINE1)));

  EXPECT_EQ(EMAIL_ADDRESS, forms[1]->field(0)->server_type());
  EXPECT_THAT(forms[1]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(EMAIL_ADDRESS)));

  EXPECT_EQ(NO_SERVER_DATA, forms[1]->field(1)->server_type());
  EXPECT_THAT(forms[1]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(NO_SERVER_DATA)));
}

// Tests ParseApiQueryResponse when the payload cannot be parsed to an
// AutofillQueryResponse where we expect an early return of the function.
TEST_F(FormStructureTestImpl,
       ParseApiQueryResponseWhenCannotParseProtoFromString) {
  // Make form 1 data.
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "email";
  field.label = u"emailaddress";
  field.name = u"emailaddress";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add form to the vector needed by the response parsing function.
  FormStructure form_structure(form);
  form_structure.field(0)->set_server_predictions(
      {CreateFieldPrediction(NAME_FULL)});
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  std::string response_string = "invalid string that cannot be parsed";
  FormStructure::ParseApiQueryResponse(std::move(response_string), forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  // Verify that the form fields remain intact because ParseApiQueryResponse
  // could not parse the server's response because it was badly serialized.
  ASSERT_GE(forms[0]->field_count(), 1U);
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
}

// Tests ParseApiQueryResponse when the payload is not base64 where we expect
// an early return of the function.
TEST_F(FormStructureTestImpl, ParseApiQueryResponseWhenPayloadNotBase64) {
  // Make form 1 data.
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "email";
  field.label = u"emailaddress";
  field.name = u"emailaddress";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add form to the vector needed by the response parsing function.
  FormStructure form_structure(form);
  form_structure.field(0)->set_server_predictions(
      {CreateFieldPrediction(NAME_FULL)});
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Make a really simple serialized API response. We don't encode it in base64.
  AutofillQueryResponse api_response;
  // Make form 1 server suggestions.
  auto* form_suggestion = api_response.add_form_suggestions();
  // Here the server gives EMAIL_ADDRESS for field of the form, which should
  // override NAME_FULL that we originally put in the form field if there
  // is no issue when parsing the query response. In this test case there is an
  // issue with the encoding of the data, hence EMAIL_ADDRESS should not be
  // applied because of early exit of the parsing function.
  AddFieldPredictionToForm(form.fields[0], EMAIL_ADDRESS, form_suggestion);

  // Serialize API response.
  std::string response_string;
  ASSERT_TRUE(api_response.SerializeToString(&response_string));

  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  // Verify that the form fields remain intact because ParseApiQueryResponse
  // could not parse the server's response that was badly encoded.
  ASSERT_GE(forms[0]->field_count(), 1U);
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
}

TEST_F(FormStructureTestImpl, ParseQueryResponse_AuthorDefinedTypes) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;

  test::CreateTestFormField("email", "email", "", "text", "email", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("password", "password", "", "password",
                            "new-password", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  forms.front()->DetermineHeuristicTypes(nullptr, nullptr);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields[0], EMAIL_ADDRESS, form_suggestion);
  AddFieldPredictionToForm(form.fields[1], ACCOUNT_CREATION_PASSWORD,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  ASSERT_GE(forms[0]->field_count(), 2U);
  // Server type is parsed from the response and is the end result type.
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(0)->server_type());
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ACCOUNT_CREATION_PASSWORD, forms[0]->field(1)->server_type());
  // TODO(crbug.com/613666): Should be a properly defined type, and not
  // UNKNOWN_TYPE.
  EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(1)->Type().GetStorableType());
}

INSTANTIATE_TEST_SUITE_P(FormStructureTest,
                         ParameterizedFormStructureTest,
                         testing::Bool());

// Tests that, when the flag is off, we will not set the predicted type to
// unknown for fields that have no server data and autocomplete off, and when
// the flag is ON, we will overwrite the predicted type.
TEST_F(FormStructureTestImpl,
       NoServerData_AutocompleteOff_FlagDisabled_NoOverwrite) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = false;

  // Autocomplete Off, with server data.
  field.label = u"First Name";
  field.name = u"firstName";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Autocomplete Off, without server data.
  field.label = u"Last Name";
  field.name = u"lastName";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Autocomplete On, with server data.
  field.should_autocomplete = true;
  field.label = u"Address";
  field.name = u"address";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Autocomplete On, without server data.
  field.label = u"Country";
  field.name = u"country";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form.fields[1], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields[2], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields[3], NO_SERVER_DATA, form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  // Only NAME_LAST should be affected by the flag.
  EXPECT_EQ(NAME_LAST, forms[0]->field(1)->Type().GetStorableType());

  EXPECT_EQ(NAME_FIRST, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(3)->Type().GetStorableType());
}

// Tests that we never overwrite the CVC heuristic-predicted type, even if there
// is no server data (votes) for every CC fields.
TEST_F(FormStructureTestImpl, NoServerDataCCFields_CVC_NoOverwrite) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = false;

  // All fields with autocomplete off and no server data.
  field.label = u"Cardholder Name";
  field.name = u"fullName";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Credit Card Number";
  field.name = u"cc-number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration Date";
  field.name = u"exp-date";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"CVC";
  field.name = u"cvc";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields[0], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields[1], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields[2], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields[3], NO_SERVER_DATA, form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
            forms[0]->field(2)->Type().GetStorableType());

  // Regardless of the flag, the CVC field should not have been overwritten.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            forms[0]->field(3)->Type().GetStorableType());
}

// Tests that we never overwrite the CVC heuristic-predicted type, even if there
// is server data (votes) for every other CC fields.
TEST_F(FormStructureTestImpl, WithServerDataCCFields_CVC_NoOverwrite) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = false;

  // All fields with autocomplete off and no server data.
  field.label = u"Cardholder Name";
  field.name = u"fullName";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Credit Card Number";
  field.name = u"cc-number";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Expiration Date";
  field.name = u"exp-date";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"CVC";
  field.name = u"cvc";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields[0], CREDIT_CARD_NAME_FULL,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields[1], CREDIT_CARD_NUMBER, form_suggestion);
  AddFieldPredictionToForm(form.fields[2], CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields[3], NO_SERVER_DATA, form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes(nullptr, nullptr);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  // Regardless of the flag, the fields should not have been overwritten,
  // including the CVC field.
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
            forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            forms[0]->field(3)->Type().GetStorableType());
}

// When two fields have the same signature and the server response has multiple
// predictions for that signature, apply the server predictions in the order
// that they were received.
TEST_F(FormStructureTestImpl, ParseQueryResponse_RankEqualSignatures) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");

  test::CreateTestFormField("First Name", "name", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("Last Name", "name", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("email", "email", "", "text", "address-level2",
                            &field);
  form_data.fields.push_back(field);

  ASSERT_EQ(CalculateFieldSignatureForField(form_data.fields[0]),
            CalculateFieldSignatureForField(form_data.fields[1]));

  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[1], NAME_LAST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[2], EMAIL_ADDRESS, form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  EXPECT_EQ(NAME_FIRST, form.field(0)->server_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());
  EXPECT_EQ(EMAIL_ADDRESS, form.field(2)->server_type());
}

// When two fields have the same signature and the server response has one
// prediction, apply the prediction to every field with that signature.
TEST_F(FormStructureTestImpl,
       ParseQueryResponse_EqualSignaturesFewerPredictions) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");

  test::CreateTestFormField("First Name", "name", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("Last Name", "name", "", "text", &field);
  form_data.fields.push_back(field);

  test::CreateTestFormField("email", "email", "", "text", "address-level2",
                            &field);
  form_data.fields.push_back(field);

  ASSERT_EQ(CalculateFieldSignatureForField(form_data.fields[0]),
            CalculateFieldSignatureForField(form_data.fields[1]));

  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields[2], EMAIL_ADDRESS, form_suggestion);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(response_string, forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  EXPECT_EQ(NAME_FIRST, form.field(0)->server_type());
  // This field gets the same signature as the previous one, because they have
  // the same signature.
  EXPECT_EQ(NAME_FIRST, form.field(1)->server_type());
  EXPECT_EQ(EMAIL_ADDRESS, form.field(2)->server_type());
}

TEST_F(FormStructureTestImpl, AllowBigForms) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  // Check that the form with 250 fields are processed correctly.
  for (size_t i = 0; i < 250; ++i) {
    field.form_control_type = "text";
    field.name = u"text" + base::NumberToString16(i);
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
  }

  FormStructure form_structure(form);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;

  AutofillPageQueryRequest encoded_query;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  EXPECT_EQ(1u, encoded_signatures.size());
}

// Tests that an Autofill upload for password form with 1 field should not be
// uploaded.
TEST_F(FormStructureTestImpl, OneFieldPasswordFormShouldNotBeUpload) {
  FormData form;
  FormFieldData field;
  field.name = u"Password";
  field.form_control_type = "password";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  EXPECT_FALSE(FormStructure(form).ShouldBeUploaded());
}

// Checks that CreateForPasswordManagerUpload builds FormStructure
// which is encodable (i.e. ready for uploading).
TEST_F(FormStructureTestImpl, CreateForPasswordManagerUpload) {
  std::unique_ptr<FormStructure> form =
      FormStructure::CreateForPasswordManagerUpload(
          FormSignature(1234),
          {FieldSignature(1), FieldSignature(10), FieldSignature(100)});
  for (auto& field : *form)
    field->host_form_signature = form->form_signature();
  EXPECT_EQ(FormSignature(1234u), form->form_signature());
  ASSERT_EQ(3u, form->field_count());
  ASSERT_EQ(FieldSignature(100u), form->field(2)->GetFieldSignature());
  std::vector<AutofillUploadContents> uploads = form->EncodeUploadRequest(
      {} /* available_field_types */, false /* form_was_autofilled */,
      "" /*login_form_signature*/, true /*observed_submission*/,
      true /* is_raw_metadata_uploading_enabled */);
  ASSERT_EQ(1u, uploads.size());
}

// Tests if a new logical form is started with the second appearance of a field
// of type |FieldTypeGroup::kName|.
TEST_F(FormStructureTestImpl, NoAutocompleteSectionNames) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = u"Full Name";
  field.name = u"fullName";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Full Name";
  field.name = u"fullName";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  test_api(&form_structure)
      .SetFieldTypes({NAME_FULL, ADDRESS_HOME_COUNTRY, PHONE_HOME_NUMBER,
                      NAME_FULL, ADDRESS_HOME_COUNTRY, PHONE_HOME_NUMBER});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

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
  FormFieldData field;

  field.max_length = 10000;

  test::CreateTestFormField("Full Name", "fullName", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Mobile Number", "mobileNumber", "", "text",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "section-blue billing name", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phone", "phone", "", "text",
                            "section-blue billing tel", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Mobile Number", "mobileNumber", "", "text",
                            "section-blue billing tel", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  test_api(&form_structure)
      .SetFieldTypes({NAME_FULL, PHONE_HOME_NUMBER, PHONE_HOME_NUMBER,
                      NAME_FULL, PHONE_HOME_NUMBER, PHONE_HOME_NUMBER,
                      ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

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
  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phonetic First Name", "firstname", "", "text",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phonetic Last Name", "lastname", "", "text",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  test_api(&form_structure)
      .SetFieldTypes({NAME_FIRST, NAME_LAST, NAME_FIRST, NAME_LAST,
                      ADDRESS_HOME_COUNTRY, NAME_FIRST});

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

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
  FormFieldData field;
  uint64_t field_max_length = 10000;

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "section-blue shipping name", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text",
                            "section-blue shipping country", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "section-blue shipping name", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", "",
                            field_max_length, &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  test_api(&form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-shipping", form_structure.field(1)->section.ToString());
  EXPECT_EQ("blue-shipping", form_structure.field(2)->section.ToString());
  EXPECT_EQ("country_0_14", form_structure.field(3)->section.ToString());
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
  FormFieldData field;
  uint64_t field_max_length = 10000;

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "section-blue shipping name", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text",
                            "section-blue billing country", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Full Name", "fullName", "", "text", "",
                            field_max_length, &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", "",
                            field_max_length, &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  test_api(&form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping", form_structure.field(0)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(1)->section.ToString());
  EXPECT_EQ("blue-billing", form_structure.field(2)->section.ToString());
  EXPECT_EQ("country_0_14", form_structure.field(3)->section.ToString());
}

// Tests if a new logical form is started with the second appearance of a field
// of type |NAME_FULL|.
TEST_F(FormStructureTestImpl, SplitByNewAutocompleteSectionName) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  uint64_t field_max_length = 10000;

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "section-blue shipping name", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("City", "city", "", "text", "", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "section-blue billing name", field_max_length,
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("City", "city", "", "text", "", field_max_length,
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  test_api(&form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_CITY, NAME_FULL, ADDRESS_HOME_CITY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

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
  FormFieldData field;

  field.max_length = 10000;

  test::CreateTestFormField("Full Name", "fullName", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text",
                            "section-blue shipping country", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "section-blue billing name", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("City", "city", "", "text", "", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  test_api(&form_structure)
      .SetFieldTypes(
          {NAME_FULL, ADDRESS_HOME_COUNTRY, NAME_FULL, ADDRESS_HOME_CITY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

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
  FormFieldData field;

  field.max_length = 10000;

  test::CreateTestFormField("Full Name", "fullName", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text",
                            "section-blue shipping country", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  test_api(&form_structure).SetFieldTypes({NAME_FULL, ADDRESS_HOME_COUNTRY});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

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
  FormFieldData field;

  field.max_length = 10000;

  test::CreateTestFormField("Full Name", "fullName", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  test::CreateTestFormField("Full Name", "fullName", "", "text",
                            "shipping name", &field);
  field.is_focusable = true;  // visible
  form.fields.push_back(field);

  FormStructure form_structure(form);

  test_api(&form_structure)
      .SetFieldTypes({NAME_FULL, PHONE_HOME_NUMBER, NAME_FULL});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);

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
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = u"Full Name";
  field.name = u"fullName";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  FieldGlobalId full_name_id = field.global_id();

  field.label = u"Country";
  field.name = u"country";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Unknown";
  field.name = u"unknown";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  FieldGlobalId unknown_id = field.global_id();

  FormStructure form_structure(form);

  test_api(&form_structure)
      .SetFieldTypes(
          {CREDIT_CARD_NAME_FULL, ADDRESS_HOME_COUNTRY, UNKNOWN_TYPE});

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  test_api(&form_structure).IdentifySections(/*ignore_autocomplete=*/false);
  std::vector<FieldGlobalId> expected_result;
  // Only credit card related and unknown fields are eligible for manual
  // filling.
  expected_result.push_back(full_name_id);
  expected_result.push_back(unknown_id);

  EXPECT_EQ(expected_result,
            FormStructure::FindFieldsEligibleForManualFilling(forms));
}

// Test that experimental server predictions are not used.
TEST_F(FormStructureTestImpl, ExperimentalServerPredictionsAreSeparate) {
  FormData form_data;
  form_data.url = GURL("http://foo.com");

  // Add 6 fields.
  for (int i = 0; i < 6; i++) {
    FormFieldData field;
    field.form_control_type = "text";
    field.label = field.name = base::NumberToString16(i);
    field.unique_renderer_id = test::MakeFieldRendererId();
    form_data.fields.push_back(field);
  }

  FormStructure form(form_data);
  form.DetermineHeuristicTypes(nullptr, nullptr);

  const auto default_autofill_prediction = CreateFieldPrediction(
      NAME_FIRST, FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
  const auto default_password_prediction = CreateFieldPrediction(
      USERNAME, FieldPrediction::SOURCE_PASSWORDS_DEFAULT);
  const auto experimental_prediction = CreateFieldPrediction(
      EMAIL_ADDRESS, FieldPrediction::SOURCE_ALL_APPROVED_EXPERIMENTS);
  const auto null_prediction = CreateFieldPrediction(
      NO_SERVER_DATA, FieldPrediction::SOURCE_UNSPECIFIED);
  const auto unknown_prediction_source = CreateFieldPrediction(
      PHONE_HOME_NUMBER, FieldPrediction::SOURCE_UNSPECIFIED);

  // Setup the query response. Default predictions must be returned by
  // `server_type()` and `server_predictions()` as provided.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // 2 default + 1 experimental predictions.
  AddFieldPredictionsToForm(
      form_data.fields[0],
      {default_autofill_prediction, default_password_prediction,
       experimental_prediction},
      form_suggestion);
  // 1 default + 1 experimental predictions.
  AddFieldPredictionsToForm(
      form_data.fields[1],
      {default_autofill_prediction, experimental_prediction}, form_suggestion);
  // 2 default predictions.
  AddFieldPredictionsToForm(
      form_data.fields[2],
      {default_autofill_prediction, default_password_prediction},
      form_suggestion);
  // 1 null + 1 experimental predictions.
  AddFieldPredictionsToForm(form_data.fields[3],
                            {null_prediction, experimental_prediction},
                            form_suggestion);
  // 1 experimental prediction. The server doesn't do that, but we can defend
  // against it anyway. The default prediction should be effectively
  // `NO_SERVER_DATA`.
  AddFieldPredictionsToForm(form_data.fields[4], {experimental_prediction},
                            form_suggestion);
  // A prediction without the source specified.
  AddFieldPredictionsToForm(form_data.fields[5], {unknown_prediction_source},
                            form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(SerializeAndEncode(response), forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr, nullptr);

  ASSERT_EQ(form.field_count(), 6U);

  auto form_as_span = base::make_span(form.begin(), form.end());
  EXPECT_THAT(
      form_as_span,
      Each(Pointee(Property(&AutofillField::server_type,
                            Not(AnyOf(experimental_prediction.type(),
                                      unknown_prediction_source.type()))))))
      << "server_type() must not return a type provided as an experiment.";

  // `server_predictions` should only return default predictions.
  EXPECT_THAT(form.field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(default_autofill_prediction),
                          EqualsPrediction(default_password_prediction)));
  EXPECT_THAT(form.field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(default_autofill_prediction)));
  EXPECT_THAT(form.field(2)->server_predictions(),
              ElementsAre(EqualsPrediction(default_autofill_prediction),
                          EqualsPrediction(default_password_prediction)));
  EXPECT_THAT(form.field(3)->server_predictions(),
              ElementsAre(EqualsPrediction(null_prediction)));
  EXPECT_THAT(form.field(4)->server_predictions(),
              ElementsAre(EqualsPrediction(null_prediction)));
  EXPECT_THAT(form.field(5)->server_predictions(),
              ElementsAre(EqualsPrediction(null_prediction)));

  // `experimental_server_predictions` should only return experimental
  // predictions.
  EXPECT_THAT(form.field(0)->experimental_server_predictions(),
              ElementsAre(EqualsPrediction(experimental_prediction)));
  EXPECT_THAT(form.field(1)->experimental_server_predictions(),
              ElementsAre(EqualsPrediction(experimental_prediction)));
  EXPECT_THAT(form.field(2)->experimental_server_predictions(), IsEmpty());
  EXPECT_THAT(form.field(3)->experimental_server_predictions(),
              ElementsAre(EqualsPrediction(experimental_prediction)));
  EXPECT_THAT(form.field(4)->experimental_server_predictions(),
              ElementsAre(EqualsPrediction(experimental_prediction)));
  EXPECT_THAT(form.field(5)->experimental_server_predictions(), IsEmpty());
}

// Tests that ParseFieldTypesWithPatterns() sets (only) the PatternSource.
TEST_P(FormStructureTest_ForPatternSource, ParseFieldTypesWithPatterns) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormStructure form_structure(form);
  test_api(&form_structure).ParseFieldTypesWithPatterns(pattern_source());
  ASSERT_THAT(test_api(&form_structure).fields(), Not(IsEmpty()));

  auto get_heuristic_type = [&](const AutofillField& field) {
    return field.heuristic_type(pattern_source());
  };
  EXPECT_THAT(
      test_api(&form_structure).fields(),
      Each(Pointee(ResultOf(get_heuristic_type,
                            AllOf(Not(NO_SERVER_DATA), Not(UNKNOWN_TYPE))))));

  for (PatternSource other_pattern_source : other_pattern_sources()) {
    auto get_other_pattern_heuristic_type = [&](const AutofillField& field) {
      return field.heuristic_type(other_pattern_source);
    };
    EXPECT_THAT(test_api(&form_structure).fields(),
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
    field.form_control_type = "text";
    field.name = name;
    field.unique_renderer_id = test::MakeFieldRendererId();
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
      result.push_back(base::invoke(fun, *field));
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

}  // namespace autofill
