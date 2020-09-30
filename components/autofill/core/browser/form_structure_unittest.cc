// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stddef.h>

#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/signatures.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace autofill {

using features::kAutofillEnforceMinRequiredFieldsForHeuristics;
using features::kAutofillEnforceMinRequiredFieldsForQuery;
using features::kAutofillEnforceMinRequiredFieldsForUpload;
using features::kAutofillLabelAffixRemoval;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

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

void AddFieldSuggestionToForm(
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion,
    autofill::FormFieldData field_data,
    ServerFieldType field_type) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  field_suggestion->set_primary_type_prediction(field_type);
}

}  // namespace

class FormStructureTestImpl : public test::FormStructureTest {
 public:
  static std::string Hash64Bit(const std::string& str) {
    return base::NumberToString(StrToHash64Bit(str));
  }

  void SetUp() override {
    // By default this trial is enabled on tests.
    EnableAutofillMetadataFieldTrial();
  }

 protected:
  void InitFeature(base::test::ScopedFeatureList* feature_list,
                   const base::Feature& feature,
                   bool is_enabled) {
    if (is_enabled)
      feature_list->InitAndEnableFeature(feature);
    else
      feature_list->InitAndDisableFeature(feature);
  }

  // Single field forms are not parseable iff all of the minimum required field
  // values are enforced.
  void CheckFormShouldBeParsed(const char* trace_message,
                               const FormData form,
                               bool expected_if_all_enforced,
                               bool expected_if_not_all_enforced) {
    SCOPED_TRACE(trace_message);
    for (bool enforce_min_for_heuristics : {true, false}) {
      base::test::ScopedFeatureList heuristics, query, upload;
      InitFeature(&heuristics, kAutofillEnforceMinRequiredFieldsForHeuristics,
                  enforce_min_for_heuristics);
      for (bool enforce_min_for_query : {true, false}) {
        base::test::ScopedFeatureList heuristics, query, upload;
        InitFeature(&query, kAutofillEnforceMinRequiredFieldsForQuery,
                    enforce_min_for_query);
        for (bool enforce_min_for_upload : {true, false}) {
          base::test::ScopedFeatureList heuristics, query, upload;
          InitFeature(&upload, kAutofillEnforceMinRequiredFieldsForUpload,
                      enforce_min_for_upload);
          bool all_enforced = enforce_min_for_heuristics &&
                              enforce_min_for_query && enforce_min_for_upload;
          FormStructure form_structure(form);
          if (all_enforced) {
            EXPECT_EQ(expected_if_all_enforced,
                      form_structure.ShouldBeParsed());
          } else {
            EXPECT_EQ(expected_if_not_all_enforced,
                      form_structure.ShouldBeParsed())
                << "heuristics:" << enforce_min_for_heuristics << "; "
                << "query:" << enforce_min_for_query << "; "
                << "upload:" << enforce_min_for_upload;
          }
        }
      }
    }
  }

  bool FormIsAutofillable(const FormData& form, bool enforce_min_fields) {
    base::test::ScopedFeatureList feature_list;
    InitFeature(&feature_list, kAutofillEnforceMinRequiredFieldsForHeuristics,
                enforce_min_fields);
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes();
    return form_structure.IsAutofillable();
  }

  bool FormShouldRunHeuristics(const FormData& form, bool enforce_min_fields) {
    base::test::ScopedFeatureList feature_list;
    InitFeature(&feature_list, kAutofillEnforceMinRequiredFieldsForHeuristics,
                enforce_min_fields);
    FormStructure form_structure(form);
    return form_structure.ShouldRunHeuristics();
  }

  bool FormShouldBeQueried(const FormData& form, bool enforce_min_fields) {
    base::test::ScopedFeatureList feature_list;
    InitFeature(&feature_list, kAutofillEnforceMinRequiredFieldsForQuery,
                enforce_min_fields);
    FormStructure form_structure(form);
    return form_structure.ShouldBeQueried();
  }

  bool FormShouldBeUploaded(const FormData& form, bool enforce_min_fields) {
    base::test::ScopedFeatureList feature_list;
    InitFeature(&feature_list, kAutofillEnforceMinRequiredFieldsForUpload,
                enforce_min_fields);
    FormStructure form_structure(form);
    return form_structure.ShouldBeUploaded();
  }

  void DisableAutofillMetadataFieldTrial() {
    field_trial_ = nullptr;
    scoped_feature_list_.Reset();
    scoped_feature_list_.Init();
  }

  void SetUpForEncoder() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        // Enabled.
        {features::kAutofillMetadataUploads},
        // Disabled.
        {});
  }

 private:
  void EnableAutofillMetadataFieldTrial() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.Init();
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        "AutofillFieldMetadata", "Enabled");
    field_trial_->group();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::FieldTrial> field_trial_;
};

class ParameterizedFormStructureTest
    : public FormStructureTestImpl,
      public testing::WithParamInterface<bool> {};

TEST_F(FormStructureTestImpl, FieldCount) {
  CheckFormStructureTestData({{{.description_for_logging = "FieldCount",
                                .fields = {{.role = ServerFieldType::USERNAME},
                                           {.label = "Password",
                                            .name = "password",
                                            .form_control_type = "password"},
                                           {.label = "Submit",
                                            .name = "",
                                            .form_control_type = "submit"},
                                           {.label = "address1",
                                            .name = "address1",
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
         .fields =
             {{.role = ServerFieldType::USERNAME},
              {.label = "Password",
               .name = "password",
               .form_control_type = "password"},
              {.role = ServerFieldType::EMAIL_ADDRESS},
              {.role = ServerFieldType::ADDRESS_HOME_CITY},
              {.role = ServerFieldType::ADDRESS_HOME_STATE,
               .form_control_type = "select-one"},
              {.label = "Submit", .name = "", .form_control_type = "submit"}}},
        {
            .determine_heuristic_type = true,
            .autofill_count = 3,
        },
        {}},

       {{.description_for_logging = "AutofillCountWithNonFillableField",
         .fields =
             {{.role = ServerFieldType::USERNAME},
              {.label = "Password",
               .name = "password",
               .form_control_type = "password"},
              {.role = ServerFieldType::EMAIL_ADDRESS},
              {.role = ServerFieldType::ADDRESS_HOME_CITY},
              {.role = ServerFieldType::ADDRESS_HOME_STATE,
               .form_control_type = "select-one"},
              {.label = "Submit", .name = "", .form_control_type = "submit"},
              {.label = "address1",
               .name = "address1",
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
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  // With min required fields enabled.
  EXPECT_FALSE(FormIsAutofillable(form, true));   // Min enforced.
  EXPECT_FALSE(FormIsAutofillable(form, false));  // Min not enforced.

  // Add a password field. The form should be picked up by the password but
  // not by autofill.
  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form, true));   // Min enforced.
  EXPECT_FALSE(FormIsAutofillable(form, false));  // Min not enforced.

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form, true));  // Min enforced.
  EXPECT_TRUE(FormIsAutofillable(form, false));  // Min not enforced.

  // Add an auto-fillable fields. With just one auto-fillable field, this should
  // be picked up by autofill only if there is no minimum field enforcement.
  field.label = ASCIIToUTF16("Address Line 1");
  field.name = ASCIIToUTF16("address1");
  field.form_control_type = "text";
  form.fields.push_back(field);

  EXPECT_FALSE(FormIsAutofillable(form, true));  // Min enforced.
  EXPECT_TRUE(FormIsAutofillable(form, false));  // Min not enforced.

  // We now have three auto-fillable fields. It's always autofillable.
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);

  EXPECT_TRUE(FormIsAutofillable(form, true));   // Min enforced.
  EXPECT_TRUE(FormIsAutofillable(form, false));  // Min not enforced.

  // The target cannot include http(s)://*/search...
  form.action = GURL("http://google.com/search?q=hello");

  EXPECT_FALSE(FormIsAutofillable(form, true));   // Min enforced.
  EXPECT_FALSE(FormIsAutofillable(form, false));  // Min not enforced.

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");

  EXPECT_TRUE(FormIsAutofillable(form, true));   // Min enforced.
  EXPECT_TRUE(FormIsAutofillable(form, false));  // Min not enforced.
}

TEST_F(FormStructureTestImpl, ShouldBeParsed) {
  FormData form;
  form.url = GURL("http://www.foo.com/");

  // Start with a single checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.name = ASCIIToUTF16("radiobtn");
  checkable_field.form_control_type = "radio";
  form.fields.push_back(checkable_field);

  // A form with a single checkable field isn't interesting.
  CheckFormShouldBeParsed("one checkable", form, false, false);

  // Add a second checkable field.
  checkable_field.name = ASCIIToUTF16("checkbox");
  checkable_field.form_control_type = "checkbox";
  form.fields.push_back(checkable_field);

  // A form with a only checkable fields isn't interesting.
  CheckFormShouldBeParsed("two checkable", form, false, false);

  // Add a text field.
  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  // Single text field forms shouldn't be parsed if all of the minimums are
  // enforced but should be parsed if ANY of the minimums is not enforced.
  CheckFormShouldBeParsed("username", form, false, true);

  // We now have three text fields, though only two are auto-fillable.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  // Three text field forms should always be parsed.
  CheckFormShouldBeParsed("three field", form, true, true);

  // The target cannot include http(s)://*/search...
  form.action = GURL("http://google.com/search?q=hello");
  CheckFormShouldBeParsed("search path", form, false, false);

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");
  CheckFormShouldBeParsed("search domain", form, true, true);

  // The form need only have three fields, but at least one must be a text
  // field.
  form.fields.clear();

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  field.form_control_type = "select-one";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  form.fields.push_back(field);

  CheckFormShouldBeParsed("text + selects", form, true, true);

  // Now, no text fields.
  form.fields[0].form_control_type = "select-one";
  CheckFormShouldBeParsed("only selects", form, false, false);

  // We have only one field, which is password.
  form.fields.clear();
  field.label = ASCIIToUTF16("Password");
  field.name = ASCIIToUTF16("pw");
  field.form_control_type = "password";
  form.fields.push_back(field);
  CheckFormShouldBeParsed("password", form, false, true);

  // We have two fields, which are passwords, should be parsed.
  field.label = ASCIIToUTF16("New password");
  field.name = ASCIIToUTF16("new_pw");
  field.form_control_type = "password";
  form.fields.push_back(field);
  CheckFormShouldBeParsed("new password", form, true, true);
}

TEST_F(FormStructureTestImpl, ShouldBeParsed_BadScheme) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  FormFieldData field;

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("name");
  field.form_control_type = "text";
  field.autocomplete_attribute = "name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.form_control_type = "text";
  field.autocomplete_attribute = "address-line1";
  form.fields.push_back(field);

  // Baseline, HTTP should work.
  form.url = GURL("http://wwww.foo.com/myform");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Baseline, HTTPS should work.
  form.url = GURL("https://wwww.foo.com/myform");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Chrome internal urls shouldn't be parsed.
  form.url = GURL("chrome://settings");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // FTP urls shouldn't be parsed.
  form.url = GURL("ftp://ftp.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // Blob urls shouldn't be parsed.
  form.url = GURL("blob://blob.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // About urls shouldn't be parsed.
  form.url = GURL("about://about.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
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

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("name");
  field.form_control_type = "name";
  field.autocomplete_attribute = "name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("Address");
  field.form_control_type = "select-one";
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_TRUE(form_structure->ShouldBeParsed());
}

// Tests that ShouldBeParsed returns true for a form containing less than three
// fields if at least one has an autocomplete attribute.
TEST_F(FormStructureTestImpl, DetermineHeuristicTypes_AutocompleteFalse) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "DetermineHeuristicTypes_AutocompleteFalse",
         .fields = {{.label = "Name",
                     .name = "name",
                     .autocomplete_attribute = "false"},
                    {.role = ServerFieldType::EMAIL_ADDRESS,
                     .autocomplete_attribute = "false"},
                    {.role = ServerFieldType::ADDRESS_HOME_STATE,
                     .autocomplete_attribute = "false",
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
         .fields =
             {{.role = ServerFieldType::NAME_FIRST},
              {.role = ServerFieldType::NAME_LAST},
              {.role = ServerFieldType::EMAIL_ADDRESS},
              {.role = ServerFieldType::PHONE_HOME_NUMBER},
              {.label = "Ext:", .name = "phoneextension"},
              {.label = "Address", .name = "address"},
              {.role = ServerFieldType::ADDRESS_HOME_CITY},
              {.role = ServerFieldType::ADDRESS_HOME_ZIP},
              {.label = "Submit", .name = "", .form_control_type = "submit"}}},
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
         .fields = {{.label = "",
                     .name = "field1",
                     .autocomplete_attribute = "given-name"},
                    {.label = "",
                     .name = "field2",
                     .autocomplete_attribute = "family-name"},
                    {.label = "",
                     .name = "field3",
                     .autocomplete_attribute = "email"},
                    {.label = "",
                     .name = "field4",
                     .autocomplete_attribute = "upi-vpa"}}},
        {
            .determine_heuristic_type = true,
            .is_autofillable = true,
            .has_author_specified_types = true,
            .has_author_specified_upi_vpa_hint = true,
            .field_count = 4,
            .autofill_count = 3,
        },
        {.expected_html_type = {HTML_TYPE_GIVEN_NAME, HTML_TYPE_FAMILY_NAME,
                                HTML_TYPE_EMAIL, HTML_TYPE_UNRECOGNIZED},
         .expected_heuristic_type = {UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE,
                                     UNKNOWN_TYPE}}}});
}

// // Verify that the heuristics are not run for non checkout formless forms.
TEST_F(FormStructureTestImpl, Heuristics_FormlessNonCheckoutForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictUnownedFieldsToFormlessCheckout);

  CheckFormStructureTestData(
      {{{.description_for_logging = "Heuristics_NonCheckoutForm",
         .fields = {{.role = ServerFieldType::NAME_FIRST,
                     .autocomplete_attribute = "given-name"},
                    {.role = ServerFieldType::NAME_LAST,
                     .autocomplete_attribute = "family-name"},
                    {.role = ServerFieldType::EMAIL_ADDRESS,
                     .autocomplete_attribute = "email"}}},
        {
            .determine_heuristic_type = true,
            .is_autofillable = true,
            .field_count = 3,
            .autofill_count = 3,
        },
        {.expected_html_type = {HTML_TYPE_GIVEN_NAME, HTML_TYPE_FAMILY_NAME,
                                HTML_TYPE_EMAIL},
         .expected_heuristic_type = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS}}},

       {{.description_for_logging = "Heuristics_FormlessNonCheckoutForm",
         .fields = {{.role = ServerFieldType::NAME_FIRST,
                     .autocomplete_attribute = "given-name"},
                    {.role = ServerFieldType::NAME_LAST,
                     .autocomplete_attribute = "family-name"},
                    {.role = ServerFieldType::EMAIL_ADDRESS,
                     .autocomplete_attribute = "email"}},
         .is_form_tag = false},
        {
            .determine_heuristic_type = true,
            .is_autofillable = true,
            .field_count = 3,
            .autofill_count = 3,
        },
        {.expected_html_type = {HTML_TYPE_GIVEN_NAME, HTML_TYPE_FAMILY_NAME,
                                HTML_TYPE_EMAIL},
         .expected_heuristic_type = {UNKNOWN_TYPE, UNKNOWN_TYPE,
                                     UNKNOWN_TYPE}}}});
}

// All fields share a common prefix which could confuse the heuristics. Test
// that the common prefixes are stripped out before running heuristics.
// This test ensures that |parseable_name| is used for heuristics.
TEST_F(FormStructureTestImpl, StripCommonNameAffix) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kAutofillLabelAffixRemoval);

  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$phone");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(ASCIIToUTF16("firstname"),
            form_structure->field(0)->parseable_name());
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(ASCIIToUTF16("lastname"),
            form_structure->field(1)->parseable_name());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(ASCIIToUTF16("email"), form_structure->field(2)->parseable_name());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(ASCIIToUTF16("phone"), form_structure->field(3)->parseable_name());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(3)->heuristic_type());
  // Submit.
  EXPECT_EQ(ASCIIToUTF16("submit"), form_structure->field(4)->parseable_name());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(4)->heuristic_type());
}

// All fields share a common prefix, but it's not stripped due to
// the |IsValidParseableName()| rule.
TEST_F(FormStructureTestImpl, StripCommonNameAffix_SmallPrefix) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kAutofillLabelAffixRemoval);

  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address 1");
  field.name = ASCIIToUTF16("address1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address 2");
  field.name = ASCIIToUTF16("address2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address 3");
  field.name = ASCIIToUTF16("address3");
  form.fields.push_back(field);

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());

  // Address 1.
  EXPECT_EQ(ASCIIToUTF16("address1"),
            form_structure->field(0)->parseable_name());
  // Address 2.
  EXPECT_EQ(ASCIIToUTF16("address2"),
            form_structure->field(1)->parseable_name());
  // Address 3
  EXPECT_EQ(ASCIIToUTF16("address3"),
            form_structure->field(2)->parseable_name());
}

// All fields share both a common prefix and suffix which could confuse the
// heuristics. Test that the common affixes are stripped out from
// |parseable_name| during |FormStructure| initialization.
TEST_F(FormStructureTestImpl, StripCommonNameAffix_PrefixAndSuffix) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kAutofillLabelAffixRemoval);

  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name =
      ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$firstname_data");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name =
      ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$lastname_data");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name =
      ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$email_data");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name =
      ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$phone_data");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name =
      ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$submit_data");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());

  // First name.
  EXPECT_EQ(ASCIIToUTF16("firstname"),
            form_structure->field(0)->parseable_name());
  // Last name.
  EXPECT_EQ(ASCIIToUTF16("lastname"),
            form_structure->field(1)->parseable_name());
  // Email.
  EXPECT_EQ(ASCIIToUTF16("email"), form_structure->field(2)->parseable_name());
  // Phone.
  EXPECT_EQ(ASCIIToUTF16("phone"), form_structure->field(3)->parseable_name());
  // Submit.
  EXPECT_EQ(ASCIIToUTF16("submit"), form_structure->field(4)->parseable_name());
}

// Only some fields share a long common long prefix, no fields share a suffix.
// Test that only the common prefixes are stripped out in |parseable_name|
// during |FormStructure| initialization.
TEST_F(FormStructureTestImpl, StripCommonNameAffix_SelectiveLongPrefix) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kAutofillLabelAffixRemoval);

  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("ctl01$ctl00$ShippingAddressCreditPhone$submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());

  // First name.
  EXPECT_EQ(ASCIIToUTF16("firstname"),
            form_structure->field(0)->parseable_name());
  // Last name.
  EXPECT_EQ(ASCIIToUTF16("lastname"),
            form_structure->field(1)->parseable_name());
  // Email.
  EXPECT_EQ(ASCIIToUTF16("email"), form_structure->field(2)->parseable_name());
  // Phone.
  EXPECT_EQ(ASCIIToUTF16("phone"), form_structure->field(3)->parseable_name());
  // Submit.
  EXPECT_EQ(ASCIIToUTF16("submit"), form_structure->field(4)->parseable_name());
}

// Only some fields share a long common short prefix, no fields share a suffix.
// Test that short uncommon prefixes are not stripped (even if there are
// enough).
TEST_F(FormStructureTestImpl,
       StripCommonNameAffix_SelectiveLongPrefixIgnoreLength) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kAutofillLabelAffixRemoval);

  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Street Name");
  field.name = ASCIIToUTF16("address_streetname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("address_housenumber");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("address_apartmentnumber");
  form.fields.push_back(field);

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());

  // First name.
  EXPECT_EQ(ASCIIToUTF16("firstname"),
            form_structure->field(0)->parseable_name());
  // Last name.
  EXPECT_EQ(ASCIIToUTF16("lastname"),
            form_structure->field(1)->parseable_name());
  // Email.
  EXPECT_EQ(ASCIIToUTF16("address_streetname"),
            form_structure->field(2)->parseable_name());
  // Phone.
  EXPECT_EQ(ASCIIToUTF16("address_housenumber"),
            form_structure->field(3)->parseable_name());
  // Submit.
  EXPECT_EQ(ASCIIToUTF16("address_apartmentnumber"),
            form_structure->field(4)->parseable_name());
}

// All fields share a common prefix which could confuse the heuristics. Test
// that the common prefix is stripped out before running heuristics.
TEST_F(FormStructureTestImpl, StripCommonNamePrefix) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "StripCommonNamePrefix",
         .fields = {{.role = ServerFieldType::NAME_FIRST,
                     .name =
                         "ctl01$ctl00$ShippingAddressCreditPhone$firstname"},
                    {.role = ServerFieldType::NAME_LAST,
                     .name = "ctl01$ctl00$ShippingAddressCreditPhone$lastname"},
                    {.role = ServerFieldType::EMAIL_ADDRESS,
                     .name = "ctl01$ctl00$ShippingAddressCreditPhone$email"},
                    {.role = ServerFieldType::PHONE_HOME_NUMBER,
                     .name = "ctl01$ctl00$ShippingAddressCreditPhone$phone"},
                    {.label = "Submit",
                     .name = "ctl01$ctl00$ShippingAddressCreditPhone$submit",
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
         .fields = {{.label = "Address 1", .name = "address1"},
                    {.label = "Address 2", .name = "address2"},
                    {.label = "Address 3", .name = "address3"}}},
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
                    {.label = "Expiration", .name = "cc_exp"},
                    {.role = ServerFieldType::ADDRESS_HOME_ZIP}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = {true, true}},
        {}}});
}

TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_Full) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_Full",
         .fields = {{.label = "Name on Card", .name = "name_on_card"},
                    {.role = ServerFieldType::CREDIT_CARD_NUMBER},
                    {.label = "Exp Month", .name = "ccmonth"},
                    {.label = "Exp Year", .name = "ccyear"},
                    {.label = "Verification", .name = "verification"},
                    {.label = "Submit",
                     .name = "submit",
                     .form_control_type = "submit"}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = {true, true}},
        {}}});
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_OnlyCCNumber) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_OnlyCCNumber",
         .fields = {{.role = ServerFieldType::CREDIT_CARD_NUMBER}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = {true, false}},
        {}}});
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTestImpl, IsCompleteCreditCardForm_AddressForm) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "IsCompleteCreditCardForm_AddressForm",
         .fields = {{.role = ServerFieldType::NAME_FIRST, .name = ""},
                    {.role = ServerFieldType::NAME_LAST, .name = ""},
                    {.role = ServerFieldType::EMAIL_ADDRESS, .name = ""},
                    {.role = ServerFieldType::PHONE_HOME_NUMBER, .name = ""},
                    {.label = "Address", .name = ""},
                    {.label = "Address", .name = ""},
                    {.role = ServerFieldType::ADDRESS_HOME_ZIP, .name = ""}}},
        {.determine_heuristic_type = true,
         .is_complete_credit_card_form = {true, false}},
        {}}});
}

// Verify that we can correctly process the 'autocomplete' attribute for phone
// number types (especially phone prefixes and suffixes).
TEST_F(FormStructureTestImpl, HeuristicsAutocompleteAttributePhoneTypes) {
  CheckFormStructureTestData(
      {{{.description_for_logging = "HeuristicsAutocompleteAttributePhoneTypes",
         .fields = {{.label = "",
                     .name = "field1",
                     .autocomplete_attribute = "tel-local"},
                    {.label = "",
                     .name = "field2",
                     .autocomplete_attribute = "tel-local-prefix"},
                    {.label = "",
                     .name = "field3",
                     .autocomplete_attribute = "tel-local-suffix"}}},
        {.determine_heuristic_type = true,
         .is_autofillable = true,
         .field_count = 3,
         .autofill_count = 3},
        {.expected_html_type = {HTML_TYPE_TEL_LOCAL, HTML_TYPE_TEL_LOCAL_PREFIX,
                                HTML_TYPE_TEL_LOCAL_SUFFIX},
         .expected_phone_part = {AutofillField::IGNORED,
                                 AutofillField::PHONE_PREFIX,
                                 AutofillField::PHONE_SUFFIX}}}});
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
                     .autocomplete_attribute = "given-name"},
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
                        .autocomplete_attribute = "unrecognized"},
                       {.label = "Middle Name", .name = "middlename"},
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
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  EXPECT_FALSE(FormShouldRunHeuristics(form, true));  // Min enforced.
  EXPECT_TRUE(FormShouldRunHeuristics(form, false));  // Min not enforced.

  EXPECT_FALSE(FormShouldBeQueried(form, true));  // Min enforced.
  EXPECT_TRUE(FormShouldBeQueried(form, false));  // Min not enforced.

  // Status Quo (Q3/2017) - Small forms not supported.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        // Enabled.
        {kAutofillEnforceMinRequiredFieldsForHeuristics,
         kAutofillEnforceMinRequiredFieldsForQuery,
         kAutofillEnforceMinRequiredFieldsForUpload},
        // Disabled.
        {});
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes();
    ASSERT_EQ(2U, form_structure.field_count());
    ASSERT_EQ(0U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(1)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(1)->server_type());
    EXPECT_FALSE(form_structure.IsAutofillable());
  }

  // Default configuration.
  {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes();
    ASSERT_EQ(2U, form_structure.field_count());
    ASSERT_EQ(0U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(1)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(1)->server_type());
    EXPECT_FALSE(form_structure.IsAutofillable());
  }

  // Enable small form heuristics.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        kAutofillEnforceMinRequiredFieldsForHeuristics);
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes();
    ASSERT_EQ(2U, form_structure.field_count());
    ASSERT_EQ(2U, form_structure.autofill_count());
    EXPECT_EQ(NAME_FIRST, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(NAME_LAST, form_structure.field(1)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(1)->server_type());
    EXPECT_TRUE(form_structure.IsAutofillable());
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
  field.form_control_type = "text";

  // Set a valid autocompelete attribute to the first field.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  EXPECT_FALSE(FormShouldRunHeuristics(form, true));  // Min enforced.
  EXPECT_TRUE(FormShouldRunHeuristics(form, false));  // Min not enforced.

  EXPECT_FALSE(FormShouldBeQueried(form, true));  // Min enforced.
  EXPECT_TRUE(FormShouldBeQueried(form, false));  // Min not enforced.

  // Status Quo (Q3/2017) - Small forms not supported.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        // Enabled.
        {kAutofillEnforceMinRequiredFieldsForHeuristics,
         kAutofillEnforceMinRequiredFieldsForQuery,
         kAutofillEnforceMinRequiredFieldsForUpload},
        // Disabled.
        {});
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes();
    ASSERT_EQ(2U, form_structure.field_count());
    ASSERT_EQ(1U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(1)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(1)->server_type());
    EXPECT_FALSE(form_structure.IsAutofillable());
  }

  // Enable small form heuristics.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        kAutofillEnforceMinRequiredFieldsForHeuristics);
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes();
    ASSERT_EQ(2U, form_structure.field_count());
    ASSERT_EQ(2U, form_structure.autofill_count());
    EXPECT_EQ(NAME_FIRST, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(NAME_LAST, form_structure.field(1)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(1)->server_type());
    EXPECT_EQ(NAME_FIRST, form_structure.field(0)->Type().GetStorableType());
    EXPECT_EQ(NAME_LAST, form_structure.field(1)->Type().GetStorableType());
    EXPECT_TRUE(form_structure.IsAutofillable());
  }

  // As a side effect of parsing small forms (if any of the heuristics, query,
  // or upload minimmums are disabled, we'll autofill fields with an
  // autocomplete attribute, even if its the only field in the form.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        kAutofillEnforceMinRequiredFieldsForUpload);
    FormData form_copy = form;
    form_copy.fields.pop_back();
    FormStructure form_structure(form_copy);
    form_structure.DetermineHeuristicTypes();
    ASSERT_EQ(1U, form_structure.field_count());
    ASSERT_EQ(1U, form_structure.autofill_count());
    EXPECT_EQ(UNKNOWN_TYPE, form_structure.field(0)->heuristic_type());
    EXPECT_EQ(NO_SERVER_DATA, form_structure.field(0)->server_type());
    EXPECT_EQ(NAME_FIRST, form_structure.field(0)->Type().GetStorableType());
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
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.autocomplete_attribute = "username";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Password");
  field.name = ASCIIToUTF16("Password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure.has_password_field());
  EXPECT_TRUE(form_structure.ShouldBeQueried());
  EXPECT_TRUE(form_structure.ShouldBeUploaded());
}

// Verify that we can correctly process sections listed in the |autocomplete|
// attribute.
TEST_F(FormStructureTestImpl, HeuristicsAutocompleteAttributeWithSections) {
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  // Some fields will have no section specified.  These fall into the default
  // section.
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  // We allow arbitrary section names.
  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);

  // "shipping" and "billing" are special section tokens that don't require the
  // "section-" prefix.
  field.autocomplete_attribute = "shipping email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "billing email";
  form.fields.push_back(field);

  // "shipping" and "billing" can be combined with other section names.
  field.autocomplete_attribute = "section-foo shipping email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "section-foo billing email";
  form.fields.push_back(field);

  // We don't do anything clever to try to coalesce sections; it's up to site
  // authors to avoid typos.
  field.autocomplete_attribute = "section--foo email";
  form.fields.push_back(field);

  // "shipping email" and "section--shipping" email should be parsed as
  // different sections.  This is only an interesting test due to how we
  // implement implicit section names from attributes like "shipping email"; see
  // the implementation for more details.
  field.autocomplete_attribute = "section--shipping email";
  form.fields.push_back(field);

  // Credit card fields are implicitly in a separate section from other fields.
  field.autocomplete_attribute = "section-foo cc-number";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure.IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(9U, form_structure.field_count());
  EXPECT_EQ(9U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to different
  // sections.
  std::set<std::string> section_names;
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
  field.form_control_type = "text";

  // Some fields will have no section specified.  These fall into the default
  // section.
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  // Specifying "section-" is equivalent to not specifying a section.
  field.autocomplete_attribute = "section- email";
  form.fields.push_back(field);

  // Invalid tokens should prevent us from setting a section name.
  field.autocomplete_attribute = "garbage section-foo email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "garbage section-bar email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "garbage shipping email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "garbage billing email";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  // Expect the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());
  EXPECT_EQ(2U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to the same
  // section.
  std::set<std::string> section_names;
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
  field.form_control_type = "text";

  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "section-foo address-line1";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  // Expect the correct number of fields.
  ASSERT_EQ(2U, form_structure.field_count());
  EXPECT_EQ(2U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to the same
  // section.
  std::set<std::string> section_names;
  for (size_t i = 0; i < 2; ++i) {
    section_names.insert(form_structure.field(i)->section);
  }
  EXPECT_EQ(1U, section_names.size());
}

// Verify that we do not override the author-specified sections from a form with
// local heuristics.
TEST_F(FormStructureTestImpl,
       HeuristicsDontOverrideAutocompleteAttributeSections) {
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.name = ASCIIToUTF16("one");
  field.autocomplete_attribute = "address-line1";
  form.fields.push_back(field);
  field.name = base::string16();
  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);
  field.name = base::string16();
  field.autocomplete_attribute = "name";
  form.fields.push_back(field);
  field.name = ASCIIToUTF16("two");
  field.autocomplete_attribute = "address-line1";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  // Expect the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());
  EXPECT_EQ(4U, form_structure.autofill_count());

  // Normally, the two separate address fields would cause us to detect two
  // separate sections; but because there is an author-specified section in this
  // form, we do not apply these usual heuristics.
  EXPECT_EQ(ASCIIToUTF16("one"), form_structure.field(0)->name);
  EXPECT_EQ(ASCIIToUTF16("two"), form_structure.field(3)->name);
  EXPECT_EQ(form_structure.field(0)->section, form_structure.field(3)->section);
}

TEST_F(FormStructureTestImpl, HeuristicsSample8) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Your First Name:");
  field.name = ASCIIToUTF16("bill.first");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Your Last Name:");
  field.name = ASCIIToUTF16("bill.last");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Street Address Line 1:");
  field.name = ASCIIToUTF16("bill.street1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Street Address Line 2:");
  field.name = ASCIIToUTF16("bill.street2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("bill.city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State (U.S.):");
  field.name = ASCIIToUTF16("bill.state");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip/Postal Code:");
  field.name = ASCIIToUTF16("BillTo.PostalCode");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country:");
  field.name = ASCIIToUTF16("bill.country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone Number:");
  field.name = ASCIIToUTF16("BillTo.Phone");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("E-mail address");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full name");
  field.name = ASCIIToUTF16("name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Company");
  field.name = ASCIIToUTF16("company");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip Code");
  field.name = ASCIIToUTF16("Home.PostalCode");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.value = ASCIIToUTF16("continue");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("First Name");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip code");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  // This is not a field we know how to process.  But we should skip over it
  // and process the other fields in the card block.
  field.label = ASCIIToUTF16("Card image");
  field.name = ASCIIToUTF16("card_image");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line3");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("shipping.address.addressLine1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("shipping.address.addressLine2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line3");
  field.name = ASCIIToUTF16("billing.address.addressLine3");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line4");
  field.name = ASCIIToUTF16("billing.address.addressLine4");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Street:");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_ads1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Suite or Apt:");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_adap");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Street address second line");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_ads2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City:");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_adct");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("address1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Floor number, suite number, etc");
  field.name = ASCIIToUTF16("address2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City:");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State/Province/Region");
  field.name = ASCIIToUTF16("State");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("First Name*:");
  field.name = ASCIIToUTF16("editBillingAddress$firstNameBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name*:");
  field.name = ASCIIToUTF16("editBillingAddress$lastNameBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Company Name:");
  field.name = ASCIIToUTF16("editBillingAddress$companyBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address*:");
  field.name = ASCIIToUTF16("editBillingAddress$addressLine1Box");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Apt/Suite :");
  field.name = ASCIIToUTF16("editBillingAddress$addressLine2Box");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City*:");
  field.name = ASCIIToUTF16("editBillingAddress$cityBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State/Province*:");
  field.name = ASCIIToUTF16("editBillingAddress$stateDropDown");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country*:");
  field.name = ASCIIToUTF16("editBillingAddress$countryDropDown");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Postal Code*:");
  field.name = ASCIIToUTF16("editBillingAddress$zipCodeBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone*:");
  field.name = ASCIIToUTF16("editBillingAddress$phoneBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email Address*:");
  field.name = ASCIIToUTF16("email$emailBox");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Phone:");
  field.name = ASCIIToUTF16("dayphone1");
  field.max_length = 0;
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("-");
  field.name = ASCIIToUTF16("dayphone2");
  field.max_length = 3;  // Size of prefix is 3.
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("-");
  field.name = ASCIIToUTF16("dayphone3");
  field.max_length = 4;  // Size of suffix is 4.  If unlimited size is
                         // passed, phone will be parsed as
                         // <country code> - <area code> - <phone>.
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("ext.:");
  field.name = ASCIIToUTF16("dayphone4");
  field.max_length = 0;
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

  // Area code.
  EXPECT_EQ(PHONE_HOME_CITY_CODE, form_structure->field(0)->heuristic_type());
  // Phone number suffix.
  EXPECT_EQ(PHONE_HOME_NUMBER, form_structure->field(1)->heuristic_type());
  // Phone number suffix.
  EXPECT_EQ(PHONE_HOME_NUMBER, form_structure->field(2)->heuristic_type());
  // Phone extension.
  EXPECT_EQ(PHONE_HOME_EXTENSION, form_structure->field(3)->heuristic_type());
}

TEST_F(FormStructureTestImpl, HeuristicsInfernoCC) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("billing_address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("expiration_month");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Year");
  field.name = ASCIIToUTF16("expiration_year");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Card number");
  field.name = ASCIIToUTF16("ccnumber");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First name");
  field.name = ASCIIToUTF16("first_name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last name");
  field.name = ASCIIToUTF16("last_name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration date");
  field.name = ASCIIToUTF16("ccexpiresmonth");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("ccexpiresyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("cvc number");
  field.name = ASCIIToUTF16("csc");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

  field.label = ASCIIToUTF16("Cardholder Name");
  field.name = ASCIIToUTF16("cc_first_name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last name");
  field.name = ASCIIToUTF16("last_name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card number");
  field.name = ASCIIToUTF16("ccnumber");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration date");
  field.name = ASCIIToUTF16("ccexpiresmonth");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("ccexpiresyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("cvc number");
  field.name = ASCIIToUTF16("csc");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
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

TEST_F(FormStructureTestImpl, EncodeQueryRequest) {
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("billing_address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("expiration_month");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Year");
  field.name = ASCIIToUTF16("expiration_year");
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  form.fields.push_back(checkable_field);
  FormStructure form_structure(form);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  std::vector<FormSignature> expected_signatures;
  expected_signatures.push_back(form_structure.form_signature());

  // Prepare the expected proto string.
  AutofillPageQueryRequest query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  test::FillQueryField(query_form->add_fields(), 412125936U, "name_on_card",
                       "text");
  test::FillQueryField(query_form->add_fields(), 1917667676U, "billing_address",
                       "text");
  test::FillQueryField(query_form->add_fields(), 2226358947U, "card_number",
                       "text");
  test::FillQueryField(query_form->add_fields(), 747221617U, "expiration_month",
                       "text");
  test::FillQueryField(query_form->add_fields(), 4108155786U, "expiration_year",
                       "text");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  AutofillPageQueryRequest encoded_query;
  std::vector<FormSignature> encoded_signatures;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  EXPECT_EQ(encoded_signatures, expected_signatures);

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

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

  encoded_query2.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

  // Add 5 address fields - this should be still a valid form.
  for (size_t i = 0; i < 5; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    form.fields.push_back(field);
  }

  FormStructure form_structure3(form);
  forms.push_back(&form_structure3);

  std::vector<FormSignature> expected_signatures3 = expected_signatures2;
  expected_signatures3.push_back(form_structure3.form_signature());

  // Add the second form to the expected proto.
  query_form = query.add_forms();
  query_form->set_signature(form_structure3.form_signature().value());

  test::FillQueryField(query_form->add_fields(), 412125936U, "name_on_card",
                       "text");
  test::FillQueryField(query_form->add_fields(), 1917667676U, "billing_address",
                       "text");
  test::FillQueryField(query_form->add_fields(), 2226358947U, "card_number",
                       "text");
  test::FillQueryField(query_form->add_fields(), 747221617U, "expiration_month",
                       "text");
  test::FillQueryField(query_form->add_fields(), 4108155786U, "expiration_year",
                       "text");
  for (int i = 0; i < 5; ++i) {
    test::FillQueryField(query_form->add_fields(), 509334676U, "address",
                         "text");
  }

  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  AutofillPageQueryRequest encoded_query3;
  std::vector<FormSignature> encoded_signatures3;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query3,
                                                &encoded_signatures3));
  EXPECT_EQ(encoded_signatures3, expected_signatures3);

  encoded_query3.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

  // |form_structures4| will have the same signature as |form_structure3|.
  form.fields.back().name = ASCIIToUTF16("address123456789");

  FormStructure form_structure4(form);
  forms.push_back(&form_structure4);

  std::vector<FormSignature> expected_signatures4 = expected_signatures3;

  AutofillPageQueryRequest encoded_query4;
  std::vector<FormSignature> encoded_signatures4;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query4,
                                                &encoded_signatures4));
  EXPECT_EQ(encoded_signatures4, expected_signatures4);

  encoded_query4.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

  FormData malformed_form(form);
  // Add 300 address fields - the form is not valid anymore, but previous ones
  // are. The result should be the same as in previous test.
  for (size_t i = 0; i < 300; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
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

  encoded_query5.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

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

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::UNVALIDATED});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::UNVALIDATED});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::INVALID});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER}, {AutofillProfile::EMPTY});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLowercaseLetter, true));
  form_structure->set_password_length_vote(10u);

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
  available_field_types.insert(ADDRESS_BILLING_LINE1);
  available_field_types.insert(ADDRESS_BILLING_LINE2);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_lowercase_letter(true);
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
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

  // Set the "autofillused" attribute to true.
  upload.set_autofill_used(true);
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload2;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload2,
      &signatures));

  encoded_upload2.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

  ////////////////
  // Setup
  ////////////////
  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_BILLING_LINE1,
         ADDRESS_BILLING_LINE2},
        {AutofillProfile::VALID, AutofillProfile::VALID,
         AutofillProfile::INVALID, AutofillProfile::INVALID});
  }

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLowercaseLetter, true));
  form_structure->set_password_length_vote(10u);

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
                        nullptr, {30U, 31U, 37U, 38U}, {2, 2, 3, 3});
  test::FillUploadField(upload.add_field(), 509334676U, "address", "text",
                        nullptr, {30U, 31U, 37U, 38U}, {2, 2, 3, 3});

  ////////////////
  // Verification
  ////////////////
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload3;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload3,
      &signatures));

  encoded_upload3.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
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

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::UNVALIDATED});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::UNVALIDATED});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::INVALID});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER}, {AutofillProfile::EMPTY});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLowercaseLetter, true));
  form_structure->set_password_length_vote(10u);

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
  available_field_types.insert(ADDRESS_BILLING_LINE1);
  available_field_types.insert(ADDRESS_BILLING_LINE2);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_lowercase_letter(true);
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

  ////////////////
  // Verification
  ////////////////
  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_NE(expected_upload_string, encoded_upload_string);
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

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::UNVALIDATED, AutofillProfile::VALID});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::UNVALIDATED, AutofillProfile::VALID});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::INVALID, AutofillProfile::VALID});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER},
      {AutofillProfile::EMPTY, AutofillProfile::VALID});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID, AutofillProfile::VALID});
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID, AutofillProfile::VALID});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLowercaseLetter, true));
  form_structure->set_password_length_vote(10u);

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
  available_field_types.insert(ADDRESS_BILLING_LINE1);
  available_field_types.insert(ADDRESS_BILLING_LINE2);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_lowercase_letter(true);
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

  ////////////////
  // Verification
  ////////////////
  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.form_control_type = "number";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {PHONE_HOME_WHOLE_NUMBER});
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_COUNTRY});
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_COUNTRY});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLowercaseLetter, true));
  form_structure->set_password_length_vote(10u);
  form_structure->set_submission_event(
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  std::vector<FormSignature> expected_signatures;
  expected_signatures.push_back(form_structure->form_signature());

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(ADDRESS_BILLING_LINE1);
  available_field_types.insert(ADDRESS_BILLING_LINE2);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_submission_event(AutofillUploadContents::HTML_FORM_SUBMISSION);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_lowercase_letter(true);
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload,
      &signatures));
  EXPECT_EQ(signatures, expected_signatures);

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

  // Set the "autofillused" attribute to true.
  upload.set_autofill_used(true);
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload2;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload2,
      &signatures));
  EXPECT_EQ(signatures, expected_signatures);

  encoded_upload2.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_BILLING_LINE1,
         ADDRESS_BILLING_LINE2});
  }

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasLowercaseLetter, true));
  form_structure->set_password_length_vote(10u);
  form_structure->set_submission_event(
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  expected_signatures[0] = form_structure->form_signature();

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
                        nullptr, {31U, 37U, 38U});
  test::FillUploadField(upload.mutable_field(6), 509334676U, "address", "text",
                        nullptr, {31U, 37U, 38U});

  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload3;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload3,
      &signatures));
  EXPECT_EQ(signatures, expected_signatures);

  encoded_upload3.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
  // Add 300 address fields - now the form is invalid, as it has too many
  // fields.
  for (size_t i = 0; i < 300; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_BILLING_LINE1,
         ADDRESS_BILLING_LINE2});
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

  AutofillUploadContents encoded_upload4;
  EXPECT_FALSE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload4,
      &signatures));
}

TEST_F(FormStructureTestImpl,
       EncodeUploadRequestWithAdditionalPasswordFormSignature) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {USERNAME});
  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ACCOUNT_CREATION_PASSWORD});
  form_structure = std::make_unique<FormStructure>(form);

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);

    if (form_structure->field(i)->name == ASCIIToUTF16("password")) {
      form_structure->field(i)->set_generation_type(
          AutofillUploadContents::Field::
              MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM);
      form_structure->field(i)->set_generated_password_changed(true);
    }
    if (form_structure->field(i)->name == ASCIIToUTF16("username")) {
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
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, "42", true, &encoded_upload, &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithAutocomplete) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
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
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  std::vector<FormSignature> signatures;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequestWithPropertiesMask) {
  DisableAutofillMetadataFieldTrial();

  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.name_attribute = field.name;
  field.id_attribute = ASCIIToUTF16("first_name");
  field.autocomplete_attribute = "given-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  field.properties_mask = FieldPropertiesFlags::kHadFocus;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.name_attribute = field.name;
  field.id_attribute = ASCIIToUTF16("last_name");
  field.autocomplete_attribute = "family-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  field.properties_mask =
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.name_attribute = field.name;
  field.id_attribute = ASCIIToUTF16("e-mail");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  field.css_classes = ASCIIToUTF16("class1 class2");
  field.properties_mask =
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
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
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_ObservedSubmissionFalse) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.name_attribute = field.name;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.name_attribute = field.name;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.name_attribute = field.name;
  field.form_control_type = "email";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
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
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(false);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(),
      /* observed_submission= */ false, &encoded_upload, &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithLabels) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  // No label for the first field.
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
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
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_WithCssClassesAndIds) {
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.css_classes = ASCIIToUTF16("last_name_field");
  field.id_attribute = ASCIIToUTF16("lastname_id");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.css_classes = ASCIIToUTF16("email_field required_field");
  field.id_attribute = ASCIIToUTF16("email_id");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));

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
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
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
  form.name = ASCIIToUTF16("myform");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
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
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequestPartialMetadata) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  // Some fields don't have "name" or "autocomplete" attributes, and some have
  // neither.
  // No label.
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.name_attribute = field.name;
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
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
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

// Sending field metadata to the server is disabled.
TEST_F(FormStructureTestImpl, EncodeUploadRequest_DisabledMetadataTrial) {
  DisableAutofillMetadataFieldTrial();

  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.name_attribute = field.name;
  field.id_attribute = ASCIIToUTF16("first_name");
  field.autocomplete_attribute = "given-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.name_attribute = field.name;
  field.id_attribute = ASCIIToUTF16("last_name");
  field.autocomplete_attribute = "family-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.name_attribute = field.name;
  field.id_attribute = ASCIIToUTF16("e-mail");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  field.css_classes = ASCIIToUTF16("class1 class2");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
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
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

// Check that we compute the "datapresent" string correctly for the given
// |available_types|.
TEST_F(FormStructureTestImpl, CheckDataPresence) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  field.name_attribute = field.name;
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last");
  field.name_attribute = field.name;
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.name_attribute = field.name;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);

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
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), true,
                                                 &encoded_upload, &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

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
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload2;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload2,
      &signatures));

  encoded_upload2.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

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
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload3;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload3,
      &signatures));

  encoded_upload3.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

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
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload4;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload4,
      &signatures));

  encoded_upload4.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

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
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload5;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload5,
      &signatures));

  encoded_upload5.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
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
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = false;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.name_attribute = field.name;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  field.name_attribute = field.name;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last");
  field.name_attribute = field.name;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.name_attribute = field.name;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_LINE1});

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_source(SubmissionSource::XHR_SUCCEEDED);
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
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

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));
  std::vector<FormSignature> signatures;

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload,
      &signatures));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

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

  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload2;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload2,
      &signatures));

  encoded_upload2.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
  // Match last field as both address home line 1 and 2.
  possible_field_types[3].insert(ADDRESS_HOME_LINE2);
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types(
          possible_field_types[form_structure->field_count() - 1]);

  // Adjust the expected upload proto.
  test::FillUploadField(upload.mutable_field(3), 509334676U, "address", "text",
                        nullptr, 31U);
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload3;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload3,
      &signatures));

  encoded_upload3.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

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

  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload4;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload4,
      &signatures));

  encoded_upload4.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_PasswordsRevealed) {
  FormData form;
  form.url = GURL("http://www.foo.com/");

  // Add 3 fields, to make the form uploadable.
  FormFieldData field;
  field.name = ASCIIToUTF16("email");
  field.name_attribute = field.name;
  form.fields.push_back(field);

  field.name = ASCIIToUTF16("first");
  field.name_attribute = field.name;
  form.fields.push_back(field);

  field.name = ASCIIToUTF16("last");
  field.name_attribute = field.name;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_passwords_were_revealed(true);
  AutofillUploadContents upload;
  std::vector<FormSignature> signatures;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(
      {{}} /* available_field_types */, false /* form_was_autofilled */,
      std::string() /* login_form_signature */, true /* observed_submission */,
      &upload, &signatures));
  EXPECT_EQ(true, upload.passwords_revealed());
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_IsFormTag) {
  for (bool is_form_tag : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_form_tag=" << is_form_tag);

    FormData form;
    form.url = GURL("http://www.foo.com/");
    FormFieldData field;
    field.name = ASCIIToUTF16("email");
    form.fields.push_back(field);

    form.is_form_tag = is_form_tag;

    FormStructure form_structure(form);
    form_structure.set_passwords_were_revealed(true);
    AutofillUploadContents upload;
    std::vector<FormSignature> signatures;
    EXPECT_TRUE(form_structure.EncodeUploadRequest(
        {{}} /* available_field_types */, false /* form_was_autofilled */,
        std::string() /* login_form_signature */,
        true /* observed_submission */, &upload, &signatures));
    EXPECT_EQ(is_form_tag, upload.has_form_tag());
  }
}

TEST_F(FormStructureTestImpl, EncodeUploadRequest_RichMetadata) {
  SetUpForEncoder();
  struct FieldMetadata {
    const char *id, *name, *label, *placeholder, *aria_label, *aria_description,
        *css_classes;
  };

  static const FieldMetadata kFieldMetadata[] = {
      {"fname_id", "fname_name", "First Name:", "Please enter your first name",
       "Type your first name", "You can type your first name here", "blah"},
      {"lname_id", "lname_name", "Last Name:", "Please enter your last name",
       "Type your lat name", "You can type your last name here", "blah"},
      {"email_id", "email_name", "Email:", "Please enter your email address",
       "Type your email address", "You can type your email address here",
       "blah"},
      {"id_only", "", "", "", "", "", ""},
      {"", "name_only", "", "", "", "", ""},
  };

  FormData form;
  form.id_attribute = ASCIIToUTF16("form-id");
  form.url = GURL("http://www.foo.com/");
  form.button_titles = {
      std::make_pair(ASCIIToUTF16("Submit"),
                     mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
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
    form.fields.push_back(field);
  }
  RandomizedEncoder encoder("seed for testing",
                            AutofillRandomizedValue_EncodingType_ALL_BITS,
                            /*anonymous_url_collection_is_enabled*/ true);

  FormStructure form_structure(form);
  form_structure.set_randomized_encoder(
      std::make_unique<RandomizedEncoder>(encoder));

  AutofillUploadContents upload;
  std::vector<FormSignature> signatures;
  ASSERT_TRUE(form_structure.EncodeUploadRequest(
      {{}} /* available_field_types */, false /* form_was_autofilled */,
      std::string() /* login_form_signature */, true /* observed_submission */,
      &upload, &signatures));

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
            base::size(kFieldMetadata));

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
  }
}

TEST_F(FormStructureTestImpl, Metadata_OnlySendFullUrlWithUserConsent) {
  for (bool has_consent : {true, false}) {
    SCOPED_TRACE(testing::Message() << " has_consent=" << has_consent);
    SetUpForEncoder();
    FormData form;
    form.id_attribute = ASCIIToUTF16("form-id");
    form.url = GURL("http://www.foo.com/");
    form.full_url = GURL("http://www.foo.com/?foo=bar");

    // One form field needed to be valid form.
    FormFieldData field;
    field.form_control_type = "text";
    field.label = ASCIIToUTF16("email");
    field.name = ASCIIToUTF16("email");
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
    AutofillUploadContents upload = AutofillUploadContents();
    std::vector<FormSignature> signatures;
    form_structure.EncodeUploadRequest({}, true, "", true, &upload,
                                       &signatures);

    EXPECT_EQ(has_consent, upload.randomized_form_metadata().has_url());
  }
}

TEST_F(FormStructureTestImpl, CheckFormSignature) {
  // Check that form signature is created correctly.
  std::unique_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);

  // Checkable fields shouldn't affect the signature.
  field.label = ASCIIToUTF16("Select");
  field.name = ASCIIToUTF16("Select");
  field.form_control_type = "checkbox";
  field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
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

  form.name = ASCIIToUTF16("login_form");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(std::string(
                "https://login.facebook.com&login_form&email&first")),
            form_structure->FormSignatureAsStr());

  // Checks how digits are removed from field names.
  field.check_status = FormFieldData::CheckStatus::kNotCheckable;
  field.label = ASCIIToUTF16("Random Field label");
  field.name = ASCIIToUTF16("random1234");
  field.form_control_type = "text";
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Random Field label2");
  field.name = ASCIIToUTF16("random12345");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Random Field label3");
  field.name = ASCIIToUTF16("1ran12dom12345678");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Random Field label3");
  field.name = ASCIIToUTF16("12345ran123456dom123");
  form.fields.push_back(field);
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTestImpl::Hash64Bit(
                std::string("https://login.facebook.com&login_form&email&first&"
                            "random1234&random&1ran12dom&random123")),
            form_structure->FormSignatureAsStr());
}

TEST_F(FormStructureTestImpl, ToFormData) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  EXPECT_TRUE(form.SameFormAs(FormStructure(form).ToFormData()));
}

TEST_F(FormStructureTestImpl, SkipFieldTest) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("select");
  field.name = ASCIIToUTF16("select");
  field.form_control_type = "checkbox";
  field.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  field.check_status = FormFieldData::CheckStatus::kNotCheckable;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  test::FillQueryField(query_form->add_fields(), 239111655U, "username",
                       "text");
  test::FillQueryField(query_form->add_fields(), 420638584U, "email", "text");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  const FormSignature kExpectedSignature(18006745212084723782UL);

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures.front());

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

TEST_F(FormStructureTestImpl, EncodeQueryRequest_WithLabels) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  // No label on the first field.
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Enter your Email address");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Enter your Password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  std::vector<FormStructure*> forms;
  FormStructure form_structure(form);
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  test::FillQueryField(query_form->add_fields(), 239111655U, "username",
                       "text");
  test::FillQueryField(query_form->add_fields(), 420638584U, "email", "text");
  test::FillQueryField(query_form->add_fields(), 2051817934U, "password",
                       "password");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  EXPECT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

TEST_F(FormStructureTestImpl, EncodeQueryRequest_WithLongLabels) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  // No label on the first field.
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  // This label will be truncated in the XML request.
  field.label = ASCIIToUTF16(
      "Enter Your Really Really Really (Really!) Long Email Address Which We "
      "Hope To Get In Order To Send You Unwanted Publicity Because That's What "
      "Marketers Do! We Know That Your Email Address Has The Possibility Of "
      "Exceeding A Certain Number Of Characters...");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Enter your Password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  test::FillQueryField(query_form->add_fields(), 239111655U, "username",
                       "text");
  test::FillQueryField(query_form->add_fields(), 420638584U, "email", "text");
  test::FillQueryField(query_form->add_fields(), 2051817934U, "password",
                       "password");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  EXPECT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

// One name is missing from one field.
TEST_F(FormStructureTestImpl, EncodeQueryRequest_MissingNames) {
  FormData form;
  // No name set for the form.
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  // No name set for this field.
  field.name = ASCIIToUTF16("");
  field.form_control_type = "text";
  field.check_status = FormFieldData::CheckStatus::kNotCheckable;
  form.fields.push_back(field);

  FormStructure form_structure(form);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  test::FillQueryField(query_form->add_fields(), 239111655U, "username",
                       "text");
  test::FillQueryField(query_form->add_fields(), 1318412689U, nullptr, "text");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  const FormSignature kExpectedSignature(16416961345885087496UL);

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures.front());

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

// Sending field metadata to the server is disabled.
TEST_F(FormStructureTestImpl, EncodeQueryRequest_DisabledMetadataTrial) {
  DisableAutofillMetadataFieldTrial();

  FormData form;
  // No name set for the form.
  form.url = GURL("http://cool.com");
  form.action = form.url.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "text";
  field.check_status = FormFieldData::CheckStatus::kNotCheckable;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<FormSignature> encoded_signatures;
  AutofillPageQueryRequest encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());

  test::FillQueryField(query_form->add_fields(), 239111655U, nullptr, nullptr);
  test::FillQueryField(query_form->add_fields(), 3654076265U, nullptr, nullptr);

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  const FormSignature kExpectedSignature(7635954436925888745UL);

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_query,
                                                &encoded_signatures));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures.front());

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

TEST_F(FormStructureTestImpl, PossibleValues) {
  FormData form_data;
  form_data.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.autocomplete_attribute = "billing country";
  field.option_contents.push_back(ASCIIToUTF16("Down Under"));
  field.option_values.push_back(ASCIIToUTF16("AU"));
  field.option_contents.push_back(ASCIIToUTF16("Fr"));
  field.option_values.push_back(ASCIIToUTF16(""));
  field.option_contents.push_back(ASCIIToUTF16("Germany"));
  field.option_values.push_back(ASCIIToUTF16("GRMNY"));
  form_data.fields.push_back(field);
  FormStructure form_structure(form_data);

  form_structure.ParseFieldTypesFromAutocompleteAttributes();

  // All values in <option> value= or contents are returned, set to upper case.
  std::set<base::string16> possible_values =
      form_structure.PossibleValues(ADDRESS_BILLING_COUNTRY);
  EXPECT_EQ(5U, possible_values.size());
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("AU")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("FR")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("DOWN UNDER")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("GERMANY")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("GRMNY")));
  EXPECT_EQ(0U, possible_values.count(ASCIIToUTF16("Fr")));
  EXPECT_EQ(0U, possible_values.count(ASCIIToUTF16("DE")));

  // No field for the given type; empty value set.
  EXPECT_EQ(0U, form_structure.PossibleValues(ADDRESS_HOME_COUNTRY).size());

  // A freeform input (<input>) allows any value (overriding other <select>s).
  FormFieldData freeform_field;
  freeform_field.autocomplete_attribute = "billing country";
  form_data.fields.push_back(freeform_field);
  FormStructure form_structure2(form_data);
  form_structure2.ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_EQ(0U, form_structure2.PossibleValues(ADDRESS_BILLING_COUNTRY).size());
}

// Test the heuristic prediction for NAME_LAST_SECOND overrides server
// predictions.
TEST_F(FormStructureTestImpl,
       ParseQueryResponse_HeuristicsOverrideSpanishLastNameTypes) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");
  field.form_control_type = "text";

  // First name field.
  field.label = ASCIIToUTF16("Nombre");
  field.name = ASCIIToUTF16("Nombre");
  form_data.fields.push_back(field);

  // First last name field.
  // Should be identified by local heuristics.
  field.label = ASCIIToUTF16("Apellido Paterno");
  field.name = ASCIIToUTF16("apellido_paterno");
  form_data.fields.push_back(field);

  // Second last name field.
  // Should be identified by local heuristics.
  field.label = ASCIIToUTF16("Apellido Materno");
  field.name = ASCIIToUTF16("apellido materno");
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes();

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[0], NAME_FIRST);
  // Simulate a NAME_LAST classification for the two last name fields.
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[1], NAME_LAST);
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[2], NAME_LAST);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
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

  // Now disable the feature and process the query again.
  scoped_feature.Reset();
  scoped_feature.InitAndDisableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  std::vector<FormStructure*> forms2{&form};
  FormStructure::ParseApiQueryResponse(
      response_string, forms2, test::GetEncodedSignatures(forms2), nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate the heuristic and server predictions.
  EXPECT_EQ(NAME_LAST_FIRST, form.field(1)->heuristic_type());
  EXPECT_EQ(NAME_LAST_SECOND, form.field(2)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());
  EXPECT_EQ(NAME_LAST, form.field(2)->server_type());

  // Validate that the heuristic prediction does not win for the two last name
  // fields.
  EXPECT_EQ(form.field(0)->Type().GetStorableType(), NAME_FIRST);
  EXPECT_EQ(form.field(1)->Type().GetStorableType(), NAME_LAST);
  EXPECT_EQ(form.field(2)->Type().GetStorableType(), NAME_LAST);
}

// Test the heuristic prediction for ADDRESS_HOME_STREET_NAME and
// ADDRESS_HOME_HOUSE_NUMBER overrides server predictions.
TEST_F(FormStructureTestImpl,
       ParseQueryResponse_HeuristicsOverrideStreetNameAndHouseNumberTypes) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");
  field.form_control_type = "text";

  // Field for the name.
  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("Name");
  form_data.fields.push_back(field);

  // Field for the street name.
  field.label = ASCIIToUTF16("Street Name");
  field.name = ASCIIToUTF16("street_name");
  form_data.fields.push_back(field);

  // Field for the house number.
  field.label = ASCIIToUTF16("House Number");
  field.name = ASCIIToUTF16("house_number");
  form_data.fields.push_back(field);

  // Field for the postal code.
  field.label = ASCIIToUTF16("ZIP");
  field.name = ASCIIToUTF16("ZIP");
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes();

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[0], NAME_FULL);
  // Simulate ADDRESS_LINE classifications for the two last name fields.
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[1],
                           ADDRESS_HOME_LINE1);
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[2],
                           ADDRESS_HOME_LINE2);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
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

  // Now disable the feature and process the query again.
  scoped_feature.Reset();
  scoped_feature.InitAndDisableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  std::vector<FormStructure*> forms2{&form};
  FormStructure::ParseApiQueryResponse(
      response_string, forms2, test::GetEncodedSignatures(forms2), nullptr);
  ASSERT_EQ(form.field_count(), 4U);

  // Validate the heuristic and server predictions.
  EXPECT_EQ(ADDRESS_HOME_STREET_NAME, form.field(1)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_HOUSE_NUMBER, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(1)->server_type());
  EXPECT_EQ(ADDRESS_HOME_LINE2, form.field(2)->server_type());

  // Validate that the heuristic prediction does not win for the street name and
  // house number.
  EXPECT_EQ(form.field(1)->Type().GetStorableType(), ADDRESS_HOME_LINE1);
  EXPECT_EQ(form.field(2)->Type().GetStorableType(), ADDRESS_HOME_LINE2);
}

// Tests proper resolution heuristic, server and html field types when the
// server returns NO_SERVER_DATA, UNKNOWN_TYPE, and a valid type.
TEST_F(FormStructureTestImpl, ParseQueryResponse_TooManyTypes) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("fname");
  form_data.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lname");
  form_data.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.autocomplete_attribute = "address-level2";
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes();

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[0], NAME_FIRST);
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[1], NAME_LAST);
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[2],
                           ADDRESS_HOME_LINE1);
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      EMAIL_ADDRESS);
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      UNKNOWN_TYPE);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate field 0.
  EXPECT_EQ(NAME_FIRST, form.field(0)->heuristic_type());
  EXPECT_EQ(NAME_FIRST, form.field(0)->server_type());
  EXPECT_EQ(HTML_TYPE_UNSPECIFIED, form.field(0)->html_type());
  EXPECT_EQ(NAME_FIRST, form.field(0)->Type().GetStorableType());

  // Validate field 1.
  EXPECT_EQ(NAME_LAST, form.field(1)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());
  EXPECT_EQ(HTML_TYPE_UNSPECIFIED, form.field(1)->html_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->Type().GetStorableType());

  // Validate field 2. Note: HTML_TYPE_ADDRESS_LEVEL2 -> City
  EXPECT_EQ(EMAIL_ADDRESS, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(2)->server_type());
  EXPECT_EQ(HTML_TYPE_ADDRESS_LEVEL2, form.field(2)->html_type());
  EXPECT_EQ(ADDRESS_HOME_CITY, form.field(2)->Type().GetStorableType());

  // Also check the extreme case of an empty form.
  FormStructure empty_form{FormData()};
  std::vector<FormStructure*> empty_forms{&empty_form};
  FormStructure::ParseApiQueryResponse(response_string, empty_forms,
                                       test::GetEncodedSignatures(empty_forms),
                                       nullptr);
  ASSERT_EQ(empty_form.field_count(), 0U);
}

// Tests proper resolution heuristic, server and html field types when the
// server returns NO_SERVER_DATA, UNKNOWN_TYPE, and a valid type.
TEST_F(FormStructureTestImpl, ParseQueryResponse_UnknownType) {
  FormData form_data;
  FormFieldData field;
  form_data.url = GURL("http://foo.com");
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("fname");
  form_data.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lname");
  form_data.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.autocomplete_attribute = "address-level2";
  form_data.fields.push_back(field);

  FormStructure form(form_data);
  form.DetermineHeuristicTypes();

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[0], UNKNOWN_TYPE);
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[1],
                           NO_SERVER_DATA);
  AddFieldSuggestionToForm(form_suggestion, form_data.fields[2],
                           ADDRESS_HOME_LINE1);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate field 0.
  EXPECT_EQ(NAME_FIRST, form.field(0)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form.field(0)->server_type());
  EXPECT_EQ(HTML_TYPE_UNSPECIFIED, form.field(0)->html_type());
  EXPECT_EQ(UNKNOWN_TYPE, form.field(0)->Type().GetStorableType());

  // Validate field 1.
  EXPECT_EQ(NAME_LAST, form.field(1)->heuristic_type());
  EXPECT_EQ(NO_SERVER_DATA, form.field(1)->server_type());
  EXPECT_EQ(HTML_TYPE_UNSPECIFIED, form.field(1)->html_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->Type().GetStorableType());

  // Validate field 2. Note: HTML_TYPE_ADDRESS_LEVEL2 -> City
  EXPECT_EQ(EMAIL_ADDRESS, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(2)->server_type());
  EXPECT_EQ(HTML_TYPE_ADDRESS_LEVEL2, form.field(2)->html_type());
  EXPECT_EQ(ADDRESS_HOME_CITY, form.field(2)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl, ParseApiQueryResponse) {
  // Make form 1 data.
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("fullname");
  field.name = ASCIIToUTF16("fullname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  // Checkable fields should be ignored in parsing
  FormFieldData checkable_field;
  checkable_field.label = ASCIIToUTF16("radio_button");
  checkable_field.form_control_type = "radio";
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  form.fields.push_back(checkable_field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Make form 2 data.
  FormData form2;
  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form2.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.push_back(&form_structure2);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  // Make form 1 suggestions.
  auto* form_suggestion = api_response.add_form_suggestions();
  auto* field0 = form_suggestion->add_field_suggestions();
  field0->set_primary_type_prediction(NAME_FULL);
  field0->set_field_signature(
      CalculateFieldSignatureForField(form.fields[0]).value());
  auto* field_prediction0 = field0->add_predictions();
  field_prediction0->set_type(NAME_FULL);
  auto* field_prediction1 = field0->add_predictions();
  field_prediction1->set_type(PHONE_FAX_COUNTRY_CODE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], ADDRESS_HOME_LINE1);
  // Make form 2 suggestions.
  form_suggestion = api_response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form2.fields[0], EMAIL_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form2.fields[1], NO_SERVER_DATA);
  // Serialize API response.
  std::string response_string;
  std::string encoded_response_string;
  ASSERT_TRUE(api_response.SerializeToString(&response_string));
  base::Base64Encode(response_string, &encoded_response_string);

  FormStructure::ParseApiQueryResponse(std::move(encoded_response_string),
                                       forms, test::GetEncodedSignatures(forms),
                                       nullptr);

  // Verify that the form fields are properly filled with data retrieved from
  // the query.
  ASSERT_GE(forms[0]->field_count(), 2U);
  ASSERT_GE(forms[1]->field_count(), 2U);
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
  ASSERT_EQ(2U, forms[0]->field(0)->server_predictions().size());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_predictions()[0].type());
  EXPECT_EQ(PHONE_FAX_COUNTRY_CODE,
            forms[0]->field(0)->server_predictions()[1].type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->server_type());
  ASSERT_EQ(1U, forms[0]->field(1)->server_predictions().size());
  EXPECT_EQ(ADDRESS_HOME_LINE1,
            forms[0]->field(1)->server_predictions()[0].type());
  EXPECT_EQ(EMAIL_ADDRESS, forms[1]->field(0)->server_type());
  ASSERT_EQ(1U, forms[1]->field(0)->server_predictions().size());
  EXPECT_EQ(EMAIL_ADDRESS, forms[1]->field(0)->server_predictions()[0].type());
  EXPECT_EQ(NO_SERVER_DATA, forms[1]->field(1)->server_type());
  ASSERT_EQ(1U, forms[1]->field(1)->server_predictions().size());
  EXPECT_EQ(0, forms[1]->field(1)->server_predictions()[0].type());
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
  field.label = ASCIIToUTF16("emailaddress");
  field.name = ASCIIToUTF16("emailaddress");
  form.fields.push_back(field);

  // Add form to the vector needed by the response parsing function.
  FormStructure form_structure(form);
  form_structure.field(0)->set_server_type(NAME_FULL);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  std::string response_string = "invalid string that cannot be parsed";
  FormStructure::ParseApiQueryResponse(std::move(response_string), forms,
                                       test::GetEncodedSignatures(forms),
                                       nullptr);

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
  field.label = ASCIIToUTF16("emailaddress");
  field.name = ASCIIToUTF16("emailaddress");
  form.fields.push_back(field);

  // Add form to the vector needed by the response parsing function.
  FormStructure form_structure(form);
  form_structure.field(0)->set_server_type(NAME_FULL);
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
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], EMAIL_ADDRESS);

  // Serialize API response.
  std::string response_string;
  ASSERT_TRUE(api_response.SerializeToString(&response_string));

  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  // Verify that the form fields remain intact because ParseApiQueryResponse
  // could not parse the server's response that was badly encoded.
  ASSERT_GE(forms[0]->field_count(), 1U);
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
}

TEST_F(FormStructureTestImpl, ParseQueryResponse_AuthorDefinedTypes) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  field.autocomplete_attribute = "new-password";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  forms.front()->DetermineHeuristicTypes();

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], EMAIL_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ACCOUNT_CREATION_PASSWORD);

  std::string response_string = SerializeAndEncode(response);
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_GE(forms[0]->field_count(), 2U);
  // Server type is parsed from the response and is the end result type.
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(0)->server_type());
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ACCOUNT_CREATION_PASSWORD, forms[0]->field(1)->server_type());
  // TODO(crbug.com/613666): Should be a properly defined type, and not
  // UNKNOWN_TYPE.
  EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(1)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl, ParseQueryResponse_RationalizeLoneField) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("fullname");
  field.name = ASCIIToUTF16("fullname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("height");
  field.name = ASCIIToUTF16("height");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], ADDRESS_HOME_LINE1);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           CREDIT_CARD_EXP_MONTH);  // Uh-oh!
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], EMAIL_ADDRESS);

  std::string response_string = SerializeAndEncode(response);

  // Test that the expiry month field is rationalized away.
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(3)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl, ParseQueryResponse_RationalizeCCName) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("fname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0],
                           CREDIT_CARD_NAME_FIRST);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           CREDIT_CARD_NAME_LAST);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], EMAIL_ADDRESS);

  std::string response_string = SerializeAndEncode(response);

  // Test that the name fields are rationalized.
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(3U, forms[0]->field_count());
  EXPECT_EQ(NAME_FIRST, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_LAST, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(2)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl, ParseQueryResponse_RationalizeMultiMonth_1) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Cardholder");
  field.name = ASCIIToUTF16("fullname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Month)");
  field.name = ASCIIToUTF16("expiry_month");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Year");
  field.name = ASCIIToUTF16("expiry_year");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Quantity");
  field.name = ASCIIToUTF16("quantity");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0],
                           CREDIT_CARD_NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], CREDIT_CARD_NUMBER);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           CREDIT_CARD_EXP_MONTH);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           CREDIT_CARD_EXP_2_DIGIT_YEAR);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4],
                           CREDIT_CARD_EXP_MONTH);  // Uh-oh!

  std::string response_string = SerializeAndEncode(response);

  // Test that the extra month field is rationalized away.
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(5U, forms[0]->field_count());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
            forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_EXP_2_DIGIT_YEAR,
            forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(4)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl, ParseQueryResponse_RationalizeMultiMonth_2) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Cardholder");
  field.name = ASCIIToUTF16("fullname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiry Date (MMYY)");
  field.name = ASCIIToUTF16("expiry");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Quantity");
  field.name = ASCIIToUTF16("quantity");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0],
                           CREDIT_CARD_NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], CREDIT_CARD_NUMBER);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           CREDIT_CARD_EXP_MONTH);  // Uh-oh!

  std::string response_string = SerializeAndEncode(response);

  // Test that the extra month field is rationalized away.
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());
  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(3)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl, SetStrippedParseableNames) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kAutofillLabelAffixRemoval);
}

TEST_F(FormStructureTestImpl, IsValidParseableName) {
  // Parseable name should not be empty.
  EXPECT_FALSE(FormStructure::IsValidParseableName(ASCIIToUTF16("")));
  // Parseable name should not be solely numerical.
  EXPECT_FALSE(FormStructure::IsValidParseableName(ASCIIToUTF16("1265125")));

  // Valid parseable name cases.
  EXPECT_TRUE(FormStructure::IsValidParseableName(ASCIIToUTF16("a23")));
  EXPECT_TRUE(FormStructure::IsValidParseableName(ASCIIToUTF16("*)&%@")));
}

TEST_F(FormStructureTestImpl, FindLongestCommonAffixLength) {
  auto String16ToStringPiece16 = [](std::vector<base::string16>& vin,
                                    std::vector<base::StringPiece16>& vout) {
    vout.clear();
    for (auto& str : vin)
      vout.push_back(str);
  };

  // Normal prefix case.
  std::vector<base::string16> strings;
  std::vector<base::StringPiece16> stringPieces;
  strings.push_back(ASCIIToUTF16("123456XXX123456789"));
  strings.push_back(ASCIIToUTF16("12345678XXX012345678_foo"));
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("1234567XXX901234567890"));
  String16ToStringPiece16(strings, stringPieces);
  size_t affixLength =
      FormStructure::FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("123456").size(), affixLength);

  // Normal suffix case.
  strings.clear();
  strings.push_back(ASCIIToUTF16("black and gold dress"));
  strings.push_back(ASCIIToUTF16("work_address"));
  strings.push_back(ASCIIToUTF16("123456XXX1234_home_address"));
  strings.push_back(ASCIIToUTF16("1234567890123456_city_address"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FormStructure::FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("dress").size(), affixLength);

  // Handles no common prefix.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("4567890123456789"));
  strings.push_back(ASCIIToUTF16("7890123456789012"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength =
      FormStructure::FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);

  // Handles no common suffix.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("4567890123456789"));
  strings.push_back(ASCIIToUTF16("7890123456789012"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FormStructure::FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);

  // Only one string, prefix case.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength =
      FormStructure::FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("1234567890").size(), affixLength);

  // Only one string, suffix case.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890"));
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FormStructure::FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("1234567890").size(), affixLength);

  // Empty vector, prefix case.
  strings.clear();
  String16ToStringPiece16(strings, stringPieces);
  affixLength =
      FormStructure::FindLongestCommonAffixLength(stringPieces, false);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);

  // Empty vector, suffix case.
  strings.clear();
  String16ToStringPiece16(strings, stringPieces);
  affixLength = FormStructure::FindLongestCommonAffixLength(stringPieces, true);
  EXPECT_EQ(ASCIIToUTF16("").size(), affixLength);
}

TEST_F(FormStructureTestImpl, FindLongestCommonPrefix) {
  // Normal case: All strings are longer than threshold; some are common.
  std::vector<base::string16> strings;
  strings.push_back(ASCIIToUTF16("1234567890123456789"));
  strings.push_back(ASCIIToUTF16("123456789012345678_foo"));
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("12345678901234567890"));
  base::string16 prefix = FormStructure::FindLongestCommonPrefix(strings);
  EXPECT_EQ(ASCIIToUTF16("1234567890123456"), prefix);

  // Handles no common prefix.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16("4567890123456789"));
  strings.push_back(ASCIIToUTF16("7890123456789012"));
  prefix = FormStructure::FindLongestCommonPrefix(strings);
  EXPECT_EQ(ASCIIToUTF16(""), prefix);

  // Some strings less than threshold length.
  strings.clear();
  strings.push_back(ASCIIToUTF16("12345678901234567890"));
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  strings.push_back(ASCIIToUTF16(""));
  strings.push_back(ASCIIToUTF16("12345"));
  strings.push_back(ASCIIToUTF16("12345678"));
  prefix = FormStructure::FindLongestCommonPrefix(strings);
  EXPECT_EQ(ASCIIToUTF16("1234567890123456"), prefix);

  // Only one string.
  strings.clear();
  strings.push_back(ASCIIToUTF16("1234567890123456"));
  prefix = FormStructure::FindLongestCommonPrefix(strings);
  EXPECT_EQ(ASCIIToUTF16("1234567890123456"), prefix);

  // Empty vector.
  strings.clear();
  prefix = FormStructure::FindLongestCommonPrefix(strings);
  EXPECT_EQ(ASCIIToUTF16(""), prefix);
}

TEST_F(FormStructureTestImpl, RationalizePhoneNumber_RunsOncePerSection) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Home Phone");
  field.name = ASCIIToUTF16("homePhoneNumber");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Cell Phone");
  field.name = ASCIIToUTF16("cellPhoneNumber");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           PHONE_HOME_WHOLE_NUMBER);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           PHONE_HOME_WHOLE_NUMBER);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  EXPECT_FALSE(form_structure.phone_rationalized_["fullName_1-default"]);
  form_structure.RationalizePhoneNumbersInSection("fullName_1-default");
  EXPECT_TRUE(form_structure.phone_rationalized_["fullName_1-default"]);
  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS, forms[0]->field(1)->server_type());

  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, forms[0]->field(2)->server_type());
  EXPECT_FALSE(forms[0]->field(2)->only_fill_when_focused());

  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, forms[0]->field(3)->server_type());
  EXPECT_TRUE(forms[0]->field(3)->only_fill_when_focused());
}

// Tests that a form that has only one address predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization.
TEST_F(FormStructureTestImpl, RationalizeRepeatedFields_OneAddress) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(3U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(2)->Type().GetStorableType());
}

// Tests that a form that has two address predicted as
// ADDRESS_HOME_STREET_ADDRESS is modified by the address rationalization to be
// ADDRESS_HOME_LINE1 and ADDRESS_HOME_LINE2 instead.
TEST_F(FormStructureTestImpl, RationalizeRepreatedFields_TwoAddresses) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE2, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(3)->Type().GetStorableType());
}

// Tests that a form that has three address lines predicted as
// ADDRESS_HOME_STREET_ADDRESS is modified by the address rationalization to be
// ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2 and ADDRESS_HOME_LINE3 instead.
TEST_F(FormStructureTestImpl, RationalizeRepreatedFields_ThreeAddresses) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(5U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE2, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE3, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(4)->Type().GetStorableType());
}

// Tests that a form that has four address lines predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization.
// This doesn't happen in real world, bc four address lines mean multiple
// sections according to the heuristics.
TEST_F(FormStructureTestImpl, RationalizeRepreatedFields_FourAddresses) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(6U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(5)->Type().GetStorableType());
}

// Tests that a form that has only one address in each section predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization.
TEST_F(FormStructureTestImpl,
       RationalizeRepreatedFields_OneAddressEachSection) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.section = "Billing";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Billing";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.section = "Billing";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.section = "Shipping";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Shipping";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.section = "Shipping";
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // Billing
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], ADDRESS_HOME_CITY);
  // Shipping
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  // Billing
  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(6U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(2)->Type().GetStorableType());
  // Shipping
  EXPECT_EQ(NAME_FULL, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(5)->Type().GetStorableType());
}

// Tests a form that has multiple sections with multiple number of address
// fields predicted as ADDRESS_HOME_STREET_ADDRESS. The last section
// doesn't happen in real world, because it is in fact two sections according to
// heuristics, and is only made for testing.
TEST_F(
    FormStructureTestImpl,
    RationalizeRepreatedFields_SectionTwoAddress_SectionThreeAddress_SectionFourAddresses) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  // Shipping
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.section = "Shipping";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Shipping";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Shipping";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.section = "Shipping";
  form.fields.push_back(field);

  // Billing
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.section = "Billing";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Billing";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Billing";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Billing";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.section = "Billing";
  form.fields.push_back(field);

  // Work address (not realistic)
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.section = "Work";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Work";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Work";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Work";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  field.section = "Work";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.section = "Work";
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], ADDRESS_HOME_CITY);

  AddFieldSuggestionToForm(form_suggestion, form.fields[4], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[6],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[7],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[8], ADDRESS_HOME_CITY);

  AddFieldSuggestionToForm(form_suggestion, form.fields[9], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[10],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[11],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[12],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[13],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[14], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(15U, forms[0]->field_count());

  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE2, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(3)->Type().GetStorableType());

  EXPECT_EQ(NAME_FULL, forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(5)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE2, forms[0]->field(6)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE3, forms[0]->field(7)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(8)->Type().GetStorableType());

  EXPECT_EQ(NAME_FULL, forms[0]->field(9)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(10)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(11)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(12)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(13)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(14)->Type().GetStorableType());
}

// Tests that a form that has only one address in each section predicted as
// ADDRESS_HOME_STREET_ADDRESS is not modified by the address rationalization,
// while the sections are previously determined by the heuristics.
TEST_F(FormStructureTestImpl,
       RationalizeRepreatedFields_MultipleSectionsByHeuristics_OneAddressEach) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // Billing
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], ADDRESS_HOME_CITY);
  // Shipping
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  // Billing
  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(6U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(2)->Type().GetStorableType());
  // Shipping
  EXPECT_EQ(NAME_FULL, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(5)->Type().GetStorableType());
}

// Tests a form that has multiple sections with multiple number of address
// fields predicted as ADDRESS_HOME_STREET_ADDRES, while the sections are
// identified by heuristics.
TEST_F(
    FormStructureTestImpl,
    RationalizeRepreatedFields_MultipleSectionsByHeuristics_TwoAddress_ThreeAddress) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  // Shipping
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  // Billing
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], ADDRESS_HOME_CITY);

  AddFieldSuggestionToForm(form_suggestion, form.fields[4], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[6],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[7],
                           ADDRESS_HOME_STREET_ADDRESS);
  AddFieldSuggestionToForm(form_suggestion, form.fields[8], ADDRESS_HOME_CITY);

  std::string response_string = SerializeAndEncode(response);
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(9U, forms[0]->field_count());

  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE2, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(3)->Type().GetStorableType());

  EXPECT_EQ(NAME_FULL, forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(5)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE2, forms[0]->field(6)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE3, forms[0]->field(7)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(8)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl,
       RationalizeRepreatedFields_StateCountry_NoRationalization) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  // First Section
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  // Second Section
  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  // Third Section
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  // Fourth Section
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_COUNTRY);
  // second section
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5], ADDRESS_HOME_STATE);
  // third section
  AddFieldSuggestionToForm(form_suggestion, form.fields[6], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[7], ADDRESS_HOME_STATE);
  // fourth section
  AddFieldSuggestionToForm(form_suggestion, form.fields[8], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[9],
                           ADDRESS_HOME_COUNTRY);

  std::string response_string = SerializeAndEncode(response);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(10U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(2)->Type().GetStorableType());
  // second section
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(NAME_FULL, forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(5)->Type().GetStorableType());
  // third section
  EXPECT_EQ(NAME_FULL, forms[0]->field(6)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(7)->Type().GetStorableType());
  // fourth section
  EXPECT_EQ(NAME_FULL, forms[0]->field(8)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(9)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl,
       RationalizeRepreatedFields_CountryStateNoHeuristics) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.section = "shipping";

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.section = "billing";

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.is_focusable = true;  // visible

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.is_focusable = true;  // visible

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  field.section = "billing-2";

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], ADDRESS_HOME_CITY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], ADDRESS_HOME_STATE);
  // second section
  AddFieldSuggestionToForm(form_suggestion, form.fields[4], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[6], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[7], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[8], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[9], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[10],
                           ADDRESS_BILLING_STATE);
  // third section
  AddFieldSuggestionToForm(form_suggestion, form.fields[11],
                           ADDRESS_BILLING_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[12], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[13],
                           ADDRESS_BILLING_STATE);

  std::string response_string = SerializeAndEncode(response);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(14U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(3)->Type().GetStorableType());
  // second section
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(5)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(6)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(7)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(8)->Type().GetStorableType());
  EXPECT_EQ(NAME_FULL, forms[0]->field(9)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(10)->Type().GetStorableType());
  // third section
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            forms[0]->field(11)->Type().GetStorableType());
  EXPECT_EQ(NAME_FULL, forms[0]->field(12)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(13)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl,
       RationalizeRepreatedFields_StateCountryWithHeuristics) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  // First Section
  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.is_focusable = true;  // visible

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("City");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state2");
  field.form_control_type = "select-one";
  field.role = FormFieldData::RoleAttribute::kPresentation;  // hidden
  form.fields.push_back(field);

  field.role = FormFieldData::RoleAttribute::kOther;  // visible

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  // Second Section
  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("City");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  // Third Section
  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("City");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state2");
  field.form_control_type = "select-one";
  field.role = FormFieldData::RoleAttribute::kPresentation;  // hidden
  form.fields.push_back(field);

  field.role = FormFieldData::RoleAttribute::kOther;  // visible

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], ADDRESS_HOME_CITY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5],
                           ADDRESS_HOME_COUNTRY);
  // second section
  AddFieldSuggestionToForm(form_suggestion, form.fields[6],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[7], ADDRESS_HOME_CITY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[8],
                           ADDRESS_BILLING_COUNTRY);
  // third section
  AddFieldSuggestionToForm(form_suggestion, form.fields[9], ADDRESS_HOME_CITY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[10],
                           ADDRESS_BILLING_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[11],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[12],
                           ADDRESS_BILLING_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[13],
                           ADDRESS_HOME_COUNTRY);

  std::string response_string = SerializeAndEncode(response);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(14U, forms[0]->field_count());
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(5)->Type().GetStorableType());
  // second section
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(6)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(7)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(8)->Type().GetStorableType());
  // third section
  EXPECT_EQ(ADDRESS_HOME_CITY, forms[0]->field(9)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(10)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(11)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            forms[0]->field(12)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            forms[0]->field(13)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl,
       RationalizeRepreatedFields_FirstFieldRationalized) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.section = "billing";

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country3");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.is_focusable = true;  // visible

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], ADDRESS_HOME_STATE);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4],
                           ADDRESS_BILLING_STATE);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(5U, forms[0]->field_count());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(NAME_FULL, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(4)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl,
       RationalizeRepreatedFields_LastFieldRationalized) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.section = "billing";

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country2");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country3");
  field.form_control_type = "select-one";
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.is_focusable = true;  // visible

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state2");
  field.is_focusable = true;  // visible
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[4],
                           ADDRESS_HOME_COUNTRY);
  AddFieldSuggestionToForm(form_suggestion, form.fields[5],
                           ADDRESS_HOME_COUNTRY);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(6U, forms[0]->field_count());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(NAME_FULL, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(5)->Type().GetStorableType());
}

INSTANTIATE_TEST_SUITE_P(All, ParameterizedFormStructureTest, testing::Bool());

// Tests that, when the flag is off, we will not set the predicted type to
// unknown for fields that have no server data and autocomplete off, and when
// the flag is ON, we will overwrite the predicted type.
TEST_P(ParameterizedFormStructureTest,
       NoServerData_AutocompleteOff_FlagDisabled_NoOverwrite) {
  base::test::ScopedFeatureList scoped_features;

  bool flag_enabled = GetParam();
  scoped_features.InitWithFeatureState(features::kAutofillOffNoServerData,
                                       flag_enabled);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = false;

  // Autocomplete Off, with server data.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstName");
  form.fields.push_back(field);

  // Autocomplete Off, without server data.
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastName");
  form.fields.push_back(field);

  // Autocomplete On, with server data.
  field.should_autocomplete = true;
  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  // Autocomplete On, without server data.
  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FIRST);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], NO_SERVER_DATA);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], NO_SERVER_DATA);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NO_SERVER_DATA);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);
  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  // Only NAME_LAST should be affected by the flag.
  EXPECT_EQ(flag_enabled ? UNKNOWN_TYPE : NAME_LAST,
            forms[0]->field(1)->Type().GetStorableType());

  EXPECT_EQ(NAME_FIRST, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(3)->Type().GetStorableType());
}

// Tests that we never overwrite the CVC heuristic-predicted type, even if there
// is no server data (votes) for every CC fields.
TEST_P(ParameterizedFormStructureTest, NoServerDataCCFields_CVC_NoOverwrite) {
  base::test::ScopedFeatureList scoped_features;

  bool flag_enabled = GetParam();
  scoped_features.InitWithFeatureState(features::kAutofillOffNoServerData,
                                       flag_enabled);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = false;

  // All fields with autocomplete off and no server data.
  field.label = ASCIIToUTF16("Cardholder Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Credit Card Number");
  field.name = ASCIIToUTF16("cc-number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("exp-date");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("CVC");
  field.name = ASCIIToUTF16("cvc");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NO_SERVER_DATA);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], NO_SERVER_DATA);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], NO_SERVER_DATA);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NO_SERVER_DATA);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  // If flag is enabled, fields should have been overwritten to Unknown.
  if (flag_enabled) {
    EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(2)->Type().GetStorableType());
  } else {
    EXPECT_EQ(CREDIT_CARD_NAME_FULL,
              forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
              forms[0]->field(2)->Type().GetStorableType());
  }

  // Regardless of the flag, the CVC field should not have been overwritten.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            forms[0]->field(3)->Type().GetStorableType());
}

// Tests that we never overwrite the CVC heuristic-predicted type, even if there
// is server data (votes) for every other CC fields.
TEST_P(ParameterizedFormStructureTest, WithServerDataCCFields_CVC_NoOverwrite) {
  base::test::ScopedFeatureList scoped_features;

  bool flag_enabled = GetParam();
  scoped_features.InitWithFeatureState(features::kAutofillOffNoServerData,
                                       flag_enabled);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = false;

  // All fields with autocomplete off and no server data.
  field.label = ASCIIToUTF16("Cardholder Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Credit Card Number");
  field.name = ASCIIToUTF16("cc-number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("exp-date");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("CVC");
  field.name = ASCIIToUTF16("cvc");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0],
                           CREDIT_CARD_NAME_FULL);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], CREDIT_CARD_NUMBER);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], NO_SERVER_DATA);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

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

struct RationalizationTypeRelationshipsTestParams {
  ServerFieldType server_type;
  ServerFieldType required_type;
};
class RationalizationFieldTypeFilterTest
    : public FormStructureTestImpl,
      public testing::WithParamInterface<ServerFieldType> {};
class RationalizationFieldTypeRelationshipsTest
    : public FormStructureTestImpl,
      public testing::WithParamInterface<
          RationalizationTypeRelationshipsTestParams> {};

INSTANTIATE_TEST_SUITE_P(All,
                         RationalizationFieldTypeFilterTest,
                         testing::Values(PHONE_HOME_COUNTRY_CODE));

INSTANTIATE_TEST_SUITE_P(All,
                         RationalizationFieldTypeRelationshipsTest,
                         testing::Values(
                             RationalizationTypeRelationshipsTestParams{
                                 PHONE_HOME_COUNTRY_CODE, PHONE_HOME_NUMBER},
                             RationalizationTypeRelationshipsTestParams{
                                 PHONE_HOME_COUNTRY_CODE,
                                 PHONE_HOME_CITY_AND_NUMBER}));

// Tests that the rationalization logic will filter out fields of type |param|
// when there is no other required type.
TEST_P(RationalizationFieldTypeFilterTest, Rationalization_Rules_Filter_Out) {
  ServerFieldType filtered_off_field = GetParam();

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = true;

  // Just adding >=3 random fields to trigger rationalization.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstName");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastName");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Something under test");
  field.name = ASCIIToUTF16("tested-thing");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FIRST);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], NAME_LAST);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2], ADDRESS_HOME_LINE1);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3], filtered_off_field);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  EXPECT_EQ(NAME_FIRST, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_LAST, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(2)->Type().GetStorableType());

  // Last field's type should have been overwritten to expected.
  EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(3)->Type().GetStorableType());
}

// Tests that the rationalization logic will not filter out fields of type
// |param| when there is another field with a required type.
TEST_P(RationalizationFieldTypeRelationshipsTest,
       Rationalization_Rules_Relationships) {
  RationalizationTypeRelationshipsTestParams test_params = GetParam();

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;
  field.should_autocomplete = true;

  // Just adding >=3 random fields to trigger rationalization.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstName");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Some field with required type");
  field.name = ASCIIToUTF16("some-name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Something under test");
  field.name = ASCIIToUTF16("tested-thing");
  form.fields.push_back(field);

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldSuggestionToForm(form_suggestion, form.fields[0], NAME_FIRST);
  AddFieldSuggestionToForm(form_suggestion, form.fields[1], NAME_LAST);
  AddFieldSuggestionToForm(form_suggestion, form.fields[2],
                           test_params.required_type);
  AddFieldSuggestionToForm(form_suggestion, form.fields[3],
                           test_params.server_type);

  std::string response_string = SerializeAndEncode(response);

  FormStructure form_structure(form);

  // Will identify the sections based on the heuristics types.
  form_structure.DetermineHeuristicTypes();

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseApiQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  EXPECT_EQ(NAME_FIRST, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_LAST, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(test_params.required_type,
            forms[0]->field(2)->Type().GetStorableType());

  // Last field's type should have been overwritten to expected.
  EXPECT_EQ(test_params.server_type,
            forms[0]->field(3)->Type().GetStorableType());
}

TEST_F(FormStructureTestImpl, AllowBigForms) {
  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  // Check that the form with 250 fields are processed correctly.
  for (size_t i = 0; i < 250; ++i) {
    field.form_control_type = "text";
    field.name = ASCIIToUTF16("text") + base::NumberToString16(i);
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /* enabled features */ {kAutofillEnforceMinRequiredFieldsForUpload},
      /* disabled features */ {kAutofillEnforceMinRequiredFieldsForQuery});
  FormData form;
  FormFieldData field;
  field.name = ASCIIToUTF16("Password");
  field.form_control_type = "password";
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
  AutofillUploadContents upload;
  std::vector<FormSignature> signatures;
  EXPECT_EQ(FormSignature(1234u), form->form_signature());
  ASSERT_EQ(3u, form->field_count());
  ASSERT_EQ(FieldSignature(100u), form->field(2)->GetFieldSignature());
  EXPECT_TRUE(form->EncodeUploadRequest(
      {} /* available_field_types */, false /* form_was_autofilled */,
      "" /*login_form_signature*/, true /*observed_submission*/, &upload,
      &signatures));
}

// Tests if a new logical form is started with the second appearance of a field
// of type |NAME|.
TEST_F(FormStructureTestImpl, NoAutocompleteSectionNames) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, ADDRESS_HOME_COUNTRY);
  form_structure.set_overall_field_type_for_testing(2, PHONE_HOME_NUMBER);
  form_structure.set_overall_field_type_for_testing(3, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(4, ADDRESS_HOME_COUNTRY);
  form_structure.set_overall_field_type_for_testing(5, PHONE_HOME_NUMBER);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());

  EXPECT_EQ("fullName_1-default", form_structure.field(0)->section);
  EXPECT_EQ("fullName_1-default", form_structure.field(1)->section);
  EXPECT_EQ("fullName_1-default", form_structure.field(2)->section);
  EXPECT_EQ("fullName_2-default", form_structure.field(3)->section);
  EXPECT_EQ("fullName_2-default", form_structure.field(4)->section);
  EXPECT_EQ("fullName_2-default", form_structure.field(5)->section);
}

// Tests that the immediate recurrence of the |PHONE_HOME_NUMBER| type does not
// lead to a section split.
TEST_F(FormStructureTestImpl, NoSplitByRecurringPhoneFieldType) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Mobile Number");
  field.name = ASCIIToUTF16("mobileNumber");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-blue billing name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.autocomplete_attribute = "section-blue billing tel";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Mobile Number");
  field.name = ASCIIToUTF16("mobileNumber");
  field.autocomplete_attribute = "section-blue billing tel";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, PHONE_HOME_NUMBER);
  form_structure.set_overall_field_type_for_testing(2, PHONE_HOME_NUMBER);
  form_structure.set_overall_field_type_for_testing(3, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(4, PHONE_BILLING_NUMBER);
  form_structure.set_overall_field_type_for_testing(5, PHONE_BILLING_NUMBER);
  form_structure.set_overall_field_type_for_testing(6, ADDRESS_HOME_COUNTRY);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(7U, form_structure.field_count());

  EXPECT_EQ("blue-billing-default", form_structure.field(0)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(1)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(2)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(3)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(4)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(5)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(6)->section);
}

// Tests if a new logical form is started with the second appearance of a field
// of type |ADDRESS_HOME_COUNTRY|.
TEST_F(FormStructureTestImpl, SplitByRecurringFieldType) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-blue shipping name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.autocomplete_attribute = "section-blue shipping country";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-blue shipping name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, ADDRESS_HOME_COUNTRY);
  form_structure.set_overall_field_type_for_testing(2, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(3, ADDRESS_HOME_COUNTRY);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping-default", form_structure.field(0)->section);
  EXPECT_EQ("blue-shipping-default", form_structure.field(1)->section);
  EXPECT_EQ("blue-shipping-default", form_structure.field(2)->section);
  EXPECT_EQ("country_2-default", form_structure.field(3)->section);
}

// Tests if a new logical form is started with the second appearance of a field
// of type |NAME_FULL| and another with the second appearance of a field of
// type |ADDRESS_HOME_COUNTRY|.
TEST_F(FormStructureTestImpl,
       SplitByNewAutocompleteSectionNameAndRecurringType) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-blue shipping name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.autocomplete_attribute = "section-blue billing country";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  FormStructure form_structure(form);

  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, ADDRESS_HOME_COUNTRY);
  form_structure.set_overall_field_type_for_testing(2, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(3, ADDRESS_HOME_COUNTRY);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping-default", form_structure.field(0)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(1)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(2)->section);
  EXPECT_EQ("country_2-default", form_structure.field(3)->section);
}

// Tests if a new logical form is started with the second appearance of a field
// of type |NAME_FULL|.
TEST_F(FormStructureTestImpl, SplitByNewAutocompleteSectionName) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-blue shipping name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-blue billing name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  FormStructure form_structure(form);

  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, ADDRESS_HOME_CITY);
  form_structure.set_overall_field_type_for_testing(2, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(3, ADDRESS_HOME_CITY);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping-default", form_structure.field(0)->section);
  EXPECT_EQ("blue-shipping-default", form_structure.field(1)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(2)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(3)->section);
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
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.autocomplete_attribute = "section-blue shipping country";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-blue billing name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  FormStructure form_structure(form);

  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, ADDRESS_HOME_COUNTRY);
  form_structure.set_overall_field_type_for_testing(2, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(3, ADDRESS_HOME_CITY);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());

  EXPECT_EQ("blue-shipping-default", form_structure.field(0)->section);
  EXPECT_EQ("blue-shipping-default", form_structure.field(1)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(2)->section);
  EXPECT_EQ("blue-billing-default", form_structure.field(3)->section);
}

// Tests if all the fields in the form belong to the same section when the
// second field has the autcomplete-section attribute set.
TEST_F(FormStructureTestImpl, FromEmptyAutocompleteSectionToDefinedOne) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.autocomplete_attribute = "section-blue shipping country";
  form.fields.push_back(field);

  FormStructure form_structure(form);

  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, ADDRESS_HOME_COUNTRY);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(2U, form_structure.field_count());

  EXPECT_EQ("blue-shipping-default", form_structure.field(0)->section);
  EXPECT_EQ("blue-shipping-default", form_structure.field(1)->section);
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
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.is_focusable = false;  // hidden
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("FullName");
  field.name = ASCIIToUTF16("fullName");
  field.is_focusable = true;  // visible
  field.autocomplete_attribute = "shipping name";
  form.fields.push_back(field);

  FormStructure form_structure(form);

  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, PHONE_HOME_NUMBER);
  form_structure.set_overall_field_type_for_testing(2, NAME_FULL);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(3U, form_structure.field_count());

  EXPECT_EQ("-shipping-default", form_structure.field(0)->section);
  EXPECT_EQ("-shipping-default", form_structure.field(1)->section);
  EXPECT_EQ("-shipping-default", form_structure.field(2)->section);
}

// Tests if the autocomplete section name other than 'shipping' and 'billing'
// are ignored.
TEST_F(FormStructureTestImpl, IgnoreAribtraryAutocompleteSectionName) {
  base::test::ScopedFeatureList enabled;
  enabled.InitAndEnableFeature(features::kAutofillUseNewSectioningMethod);

  FormData form;
  form.url = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";
  field.max_length = 10000;

  field.label = ASCIIToUTF16("Full Name");
  field.name = ASCIIToUTF16("fullName");
  field.autocomplete_attribute = "section-red ship name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.autocomplete_attribute = "section-blue shipping country";
  form.fields.push_back(field);

  FormStructure form_structure(form);

  form_structure.set_overall_field_type_for_testing(0, NAME_FULL);
  form_structure.set_overall_field_type_for_testing(1, ADDRESS_HOME_COUNTRY);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  form_structure.identify_sections_for_testing();

  // Assert the correct number of fields.
  ASSERT_EQ(2U, form_structure.field_count());

  EXPECT_EQ("blue-shipping-default", form_structure.field(0)->section);
  EXPECT_EQ("blue-shipping-default", form_structure.field(1)->section);
}

}  // namespace autofill
