// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics;
using autofill::features::kAutofillEnforceMinRequiredFieldsForQuery;
using autofill::features::kAutofillEnforceMinRequiredFieldsForUpload;
using autofill::features::kAutofillRationalizeRepeatedServerPredictions;

namespace autofill {

class FormStructureTest : public testing::Test {
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

  void DisableAutofillMetadataFieldTrial() { field_trial_list_.reset(); }

 private:
  void EnableAutofillMetadataFieldTrial() {
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(
        std::make_unique<variations::SHA1EntropyProvider>("foo")));
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        "AutofillFieldMetadata", "Enabled");
    field_trial_->group();
  }

  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;
};

TEST_F(FormStructureTest, FieldCount) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

  field.label = ASCIIToUTF16("address1");
  field.name = ASCIIToUTF16("address1");
  field.form_control_type = "text";
  field.should_autocomplete = false;
  form.fields.push_back(field);

  // The render process sends all fields to browser including fields with
  // autocomplete=off
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(4U, form_structure->field_count());
}

TEST_F(FormStructureTest, AutofillCount) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("state");
  field.name = ASCIIToUTF16("state");
  field.form_control_type = "select-one";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  // Only text and select fields that are heuristically matched are counted.
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_EQ(3U, form_structure->autofill_count());

  // Add a field with should_autocomplete=false. This should not be considered a
  // fillable field.
  field.label = ASCIIToUTF16("address1");
  field.name = ASCIIToUTF16("address1");
  field.form_control_type = "text";
  field.should_autocomplete = false;
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_EQ(4U, form_structure->autofill_count());
}

TEST_F(FormStructureTest, SourceURL) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");
  FormStructure form_structure(form);

  EXPECT_EQ(form.origin, form_structure.source_url());
}

TEST_F(FormStructureTest, IsAutofillable) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");
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

TEST_F(FormStructureTest, ShouldBeParsed) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  // Start with a single checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
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

TEST_F(FormStructureTest, ShouldBeParsed_BadScheme) {
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
  form.origin = GURL("http://wwww.foo.com/myform");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Baseline, HTTPS should work.
  form.origin = GURL("https://wwww.foo.com/myform");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_TRUE(form_structure->ShouldRunHeuristics());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  // Chrome internal urls shouldn't be parsed.
  form.origin = GURL("chrome://settings");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // FTP urls shouldn't be parsed.
  form.origin = GURL("ftp://ftp.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // Blob urls shouldn't be parsed.
  form.origin = GURL("blob://blob.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());

  // About urls shouldn't be parsed.
  form.origin = GURL("about://about.foo.com/form.html");
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  EXPECT_FALSE(form_structure->ShouldBeParsed());
  EXPECT_FALSE(form_structure->ShouldRunHeuristics());
  EXPECT_FALSE(form_structure->ShouldBeQueried());
  EXPECT_FALSE(form_structure->ShouldBeUploaded());
}

// Tests that ShouldBeParsed returns true for a form containing less than three
// fields if at least one has an autocomplete attribute.
TEST_F(FormStructureTest, ShouldBeParsed_TwoFields_HasAutocomplete) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");
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
TEST_F(FormStructureTest, DetermineHeuristicTypes_AutocompleteFalse) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");
  FormFieldData field;

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("name");
  field.form_control_type = "text";
  field.autocomplete_attribute = "false";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  field.autocomplete_attribute = "false";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  field.form_control_type = "select-one";
  field.autocomplete_attribute = "false";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->ShouldBeParsed());
  EXPECT_EQ(3U, form_structure->autofill_count());
  EXPECT_EQ(NAME_FULL, form_structure->field(0)->Type().GetStorableType());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure->field(2)->Type().GetStorableType());
}

TEST_F(FormStructureTest, HeuristicsContactInfo) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Ext:");
  field.name = ASCIIToUTF16("phoneextension");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip code");
  field.name = ASCIIToUTF16("zipcode");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(9U, form_structure->field_count());
  ASSERT_EQ(8U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(3)->heuristic_type());
  // Phone extension.
  EXPECT_EQ(PHONE_HOME_EXTENSION, form_structure->field(4)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(5)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(6)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(7)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(8)->heuristic_type());
}

// Verify that we can correctly process the |autocomplete| attribute.
TEST_F(FormStructureTest, HeuristicsAutocompleteAttribute) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = base::string16();
  field.name = ASCIIToUTF16("field1");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field2");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field3");
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field4");
  field.autocomplete_attribute = "upi-vpa";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  EXPECT_TRUE(form_structure->has_author_specified_types());
  EXPECT_TRUE(form_structure->has_author_specified_upi_vpa_hint());

  // Expect the correct number of fields.
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(HTML_TYPE_GIVEN_NAME, form_structure->field(0)->html_type());
  EXPECT_EQ(HTML_TYPE_FAMILY_NAME, form_structure->field(1)->html_type());
  EXPECT_EQ(HTML_TYPE_EMAIL, form_structure->field(2)->html_type());
  EXPECT_EQ(HTML_TYPE_UNRECOGNIZED, form_structure->field(3)->html_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(3)->heuristic_type());
}

// Verify that the heuristics are not run for non checkout formless forms.
TEST_F(FormStructureTest, Heuristics_FormlessNonCheckoutForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictUnownedFieldsToFormlessCheckout);
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name:");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name:");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email:");
  field.name = ASCIIToUTF16("email");
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // The heuristic type should be good.
  EXPECT_EQ(HTML_TYPE_GIVEN_NAME, form_structure->field(0)->html_type());
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());

  // Set the form as a formless non checkout form.
  form.is_formless_checkout = false;
  form.is_form_tag = false;

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // The heuristic type should be Unknown.
  EXPECT_EQ(HTML_TYPE_GIVEN_NAME, form_structure->field(0)->html_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(0)->heuristic_type());
}

// All fields share a common prefix which could confuse the heuristics. Test
// that the common prefix is stripped out before running heuristics.
TEST_F(FormStructureTest, StripCommonNamePrefix) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(3)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(4)->heuristic_type());
}

// All fields share a common prefix which is small enough that it is not
// stripped from the name before running the heuristics.
TEST_F(FormStructureTest, StripCommonNamePrefix_SmallPrefix) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address 3
  EXPECT_EQ(ADDRESS_HOME_LINE3, form_structure->field(2)->heuristic_type());
}

TEST_F(FormStructureTest, IsCompleteCreditCardForm_Minimal) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration");
  field.name = ASCIIToUTF16("cc_exp");
  form.fields.push_back(field);

  // Another field to reach the minimum 3.
  field.label = ASCIIToUTF16("Zip");
  field.name = ASCIIToUTF16("zip");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  EXPECT_TRUE(form_structure->IsCompleteCreditCardForm());
}

TEST_F(FormStructureTest, IsCompleteCreditCardForm_Full) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

  EXPECT_TRUE(form_structure->IsCompleteCreditCardForm());
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTest, IsCompleteCreditCardForm_OnlyCCNumber) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  EXPECT_FALSE(form_structure->IsCompleteCreditCardForm());
}

// A form with only the credit card number is not considered sufficient.
TEST_F(FormStructureTest, IsCompleteCreditCardForm_AddressForm) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  EXPECT_FALSE(form_structure->IsCompleteCreditCardForm());
}

// Verify that we can correctly process the 'autocomplete' attribute for phone
// number types (especially phone prefixes and suffixes).
TEST_F(FormStructureTest, HeuristicsAutocompleteAttributePhoneTypes) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = base::string16();
  field.name = ASCIIToUTF16("field1");
  field.autocomplete_attribute = "tel-local";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field2");
  field.autocomplete_attribute = "tel-local-prefix";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field3");
  field.autocomplete_attribute = "tel-local-suffix";
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  EXPECT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(HTML_TYPE_TEL_LOCAL, form_structure->field(0)->html_type());
  EXPECT_EQ(AutofillField::IGNORED, form_structure->field(0)->phone_part());
  EXPECT_EQ(HTML_TYPE_TEL_LOCAL_PREFIX, form_structure->field(1)->html_type());
  EXPECT_EQ(AutofillField::PHONE_PREFIX,
            form_structure->field(1)->phone_part());
  EXPECT_EQ(HTML_TYPE_TEL_LOCAL_SUFFIX, form_structure->field(2)->html_type());
  EXPECT_EQ(AutofillField::PHONE_SUFFIX,
            form_structure->field(2)->phone_part());
}

// The heuristics and server predictions should run if there are more than two
// fillable fields.
TEST_F(FormStructureTest,
       HeuristicsAndServerPredictions_BigForm_NoAutocompleteAttribute) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
}

// The heuristics and server predictions should run even if a valid autocomplete
// attribute is present in the form (if it has more that two fillable fields).
TEST_F(FormStructureTest,
       HeuristicsAndServerPredictions_ValidAutocompleteAttribute) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  // Set a valid autocomplete attribute to the first field.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  EXPECT_TRUE(form_structure->ShouldBeQueried());
  EXPECT_TRUE(form_structure->ShouldBeUploaded());

  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
}

// The heuristics and server predictions should run even if an unrecognized
// autocomplete attribute is present in the form (if it has more than two
// fillable fields).
TEST_F(FormStructureTest,
       HeuristicsAndServerPredictions_UnrecognizedAutocompleteAttribute) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  // Set an unrecognized autocomplete attribute to the first field.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Middle Name");
  field.name = ASCIIToUTF16("middlename");
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  EXPECT_TRUE(form_structure->ShouldBeQueried());

  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_MIDDLE, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(3)->heuristic_type());
}

// Tests whether the heuristics and server predictions are run for forms with
// fewer than 3 fields  and no autocomplete attributes.
TEST_F(FormStructureTest,
       HeuristicsAndServerPredictions_SmallForm_NoAutocompleteAttribute) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest,
       HeuristicsAndServerPredictions_SmallForm_ValidAutocompleteAttribute) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, PasswordFormShouldBeQueried) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  EXPECT_TRUE(form_structure.ShouldBeQueried());
  EXPECT_TRUE(form_structure.ShouldBeUploaded());
}

// Verify that we can correctly process sections listed in the |autocomplete|
// attribute.
TEST_F(FormStructureTest, HeuristicsAutocompleteAttributeWithSections) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest,
       HeuristicsAutocompleteAttributeWithSectionsDegenerate) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, HeuristicsAutocompleteAttributeWithSectionsRepeated) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, HeuristicsDontOverrideAutocompleteAttributeSections) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, HeuristicsSample8) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, HeuristicsSample6) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, HeuristicsLabelsOnly) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, HeuristicsCreditCardInfo) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, HeuristicsCreditCardInfoWithUnknownCardField) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, ThreeAddressLines) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, SurplusAddressLinesIgnored) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, ThreeAddressLinesExpedia) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, TwoAddressLinesEbay) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, HeuristicsStateWithProvince) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, HeuristicsWithBilling) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, ThreePartPhoneNumber) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, HeuristicsInfernoCC) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, HeuristicsInferCCNames_NamesNotFirst) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
TEST_F(FormStructureTest, HeuristicsInferCCNames_NamesFirst) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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

TEST_F(FormStructureTest, EncodeQueryRequest) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  form.fields.push_back(checkable_field);
  FormStructure form_structure(form);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<std::string> encoded_signatures;

  // Prepare the expected proto string.
  AutofillQueryContents query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillQueryContents::Form* query_form = query.add_form();
  query_form->set_signature(form_structure.form_signature());

  test::FillQueryField(query_form->add_field(), 412125936U, "name_on_card",
                       "text");
  test::FillQueryField(query_form->add_field(), 1917667676U, "billing_address",
                       "text");
  test::FillQueryField(query_form->add_field(), 2226358947U, "card_number",
                       "text");
  test::FillQueryField(query_form->add_field(), 747221617U, "expiration_month",
                       "text");
  test::FillQueryField(query_form->add_field(), 4108155786U, "expiration_year",
                       "text");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  const std::string kSignature1 = form_structure.FormSignatureAsStr();

  AutofillQueryContents encoded_query;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

  // Add the same form, only one will be encoded, so EncodeQueryRequest() should
  // return the same data.
  FormStructure form_structure2(form);
  forms.push_back(&form_structure2);

  AutofillQueryContents encoded_query2;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query2));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);

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

  // Add the second form to the expected proto.
  query_form = query.add_form();
  query_form->set_signature(form_structure3.form_signature());

  test::FillQueryField(query_form->add_field(), 412125936U, "name_on_card",
                       "text");
  test::FillQueryField(query_form->add_field(), 1917667676U, "billing_address",
                       "text");
  test::FillQueryField(query_form->add_field(), 2226358947U, "card_number",
                       "text");
  test::FillQueryField(query_form->add_field(), 747221617U, "expiration_month",
                       "text");
  test::FillQueryField(query_form->add_field(), 4108155786U, "expiration_year",
                       "text");
  for (int i = 0; i < 5; ++i) {
    test::FillQueryField(query_form->add_field(), 509334676U, "address",
                         "text");
  }

  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  AutofillQueryContents encoded_query3;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query3));
  ASSERT_EQ(2U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  const std::string kSignature2 = form_structure3.FormSignatureAsStr();
  EXPECT_EQ(kSignature2, encoded_signatures[1]);

  encoded_query3.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

  FormData malformed_form(form);
  // Add 150 address fields - the form is not valid anymore, but previous ones
  // are. The result should be the same as in previous test.
  for (size_t i = 0; i < 150; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    malformed_form.fields.push_back(field);
  }

  FormStructure malformed_form_structure(malformed_form);
  forms.push_back(&malformed_form_structure);
  AutofillQueryContents encoded_query4;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query4));
  ASSERT_EQ(2U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  EXPECT_EQ(kSignature2, encoded_signatures[1]);

  encoded_query4.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);

  // Check that we fail if there are only bad form(s).
  std::vector<FormStructure*> bad_forms;
  bad_forms.push_back(&malformed_form_structure);
  AutofillQueryContents encoded_query5;
  EXPECT_FALSE(FormStructure::EncodeQueryRequest(bad_forms, &encoded_signatures,
                                                 &encoded_query5));
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithMatchingValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasNumeric, true));
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_numeric(true);
  upload.set_password_length(10u);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

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

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

  // Set the "autofillused" attribute to true.
  upload.set_autofill_used(true);
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload2;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload2));

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
      std::make_pair(PasswordAttribute::kHasNumeric, true));
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
  upload.set_form_signature(form_structure->form_signature());
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
      available_field_types, false, std::string(), true, &encoded_upload3));

  encoded_upload3.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithNonMatchingValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasNumeric, true));
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_numeric(true);
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

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_NE(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithMultipleValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::VALID, AutofillProfile::VALID});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasNumeric, true));
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_numeric(true);
  upload.set_password_length(10u);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

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

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_COUNTRY});
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_password_attributes_vote(
      std::make_pair(PasswordAttribute::kHasNumeric, true));
  form_structure->set_password_length_vote(10u);
  form_structure->set_submission_event(
      PasswordForm::SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);

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
  upload.set_submission_event(AutofillUploadContents::HTML_FORM_SUBMISSION);
  upload.set_client_version("6.1.1715.1442/en (GGLL)");
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(false);
  upload.set_data_present("144200030e");
  upload.set_passwords_revealed(false);
  upload.set_password_has_numeric(true);
  upload.set_password_length(10u);
  upload.set_action_signature(15724779818122431245U);

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

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);

  // Set the "autofillused" attribute to true.
  upload.set_autofill_used(true);
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload2;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload2));

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
      std::make_pair(PasswordAttribute::kHasNumeric, true));
  form_structure->set_password_length_vote(10u);
  form_structure->set_submission_event(
      PasswordForm::SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Adjust the expected proto string.
  upload.set_form_signature(form_structure->form_signature());
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
      available_field_types, false, std::string(), true, &encoded_upload3));

  encoded_upload3.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
  // Add 150 address fields - now the form is invalid, as it has too many
  // fields.
  for (size_t i = 0; i < 150; ++i) {
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
      available_field_types, false, std::string(), true, &encoded_upload4));
}

TEST_F(FormStructureTest,
       EncodeUploadRequestWithAdditionalPasswordFormSignature) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440000000000000000802");
  upload.set_action_signature(15724779818122431245U);
  upload.set_login_form_signature(42);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

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

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  "42", true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithAutocomplete) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        "given-name", 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        "family-name", 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        "email", 9U);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequestWithPropertiesMask) {
  DisableAutofillMetadataFieldTrial();

  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.id = ASCIIToUTF16("first_name");
  field.autocomplete_attribute = "given-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  field.properties_mask = FieldPropertiesFlags::HAD_FOCUS;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.id = ASCIIToUTF16("last_name");
  field.autocomplete_attribute = "family-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  field.properties_mask =
      FieldPropertiesFlags::HAD_FOCUS | FieldPropertiesFlags::USER_TYPED;
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.id = ASCIIToUTF16("e-mail");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  field.css_classes = ASCIIToUTF16("class1 class2");
  field.properties_mask =
      FieldPropertiesFlags::HAD_FOCUS | FieldPropertiesFlags::USER_TYPED;
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

  test::FillUploadField(upload.add_field(), 3763331450U, nullptr, nullptr,
                        nullptr, 3U);
  upload.mutable_field(0)->set_properties_mask(FieldPropertiesFlags::HAD_FOCUS);
  test::FillUploadField(upload.add_field(), 3494530716U, nullptr, nullptr,
                        nullptr, 5U);
  upload.mutable_field(1)->set_properties_mask(
      FieldPropertiesFlags::HAD_FOCUS | FieldPropertiesFlags::USER_TYPED);
  test::FillUploadField(upload.add_field(), 1029417091U, nullptr, nullptr,
                        nullptr, 9U);
  upload.mutable_field(2)->set_properties_mask(
      FieldPropertiesFlags::HAD_FOCUS | FieldPropertiesFlags::USER_TYPED);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest_ObservedSubmissionFalse) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

  test::FillUploadField(upload.add_field(), 3763331450U, "firstname", "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, "email", "email",
                        nullptr, 9U);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(),
      /* observed_submission= */ false, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithLabels) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 9U);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithCssClassesAndIds) {
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.css_classes = ASCIIToUTF16("last_name_field");
  field.id = ASCIIToUTF16("lastname_id");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.css_classes = ASCIIToUTF16("email_field required_field");
  field.id = ASCIIToUTF16("email_id");
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

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

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

// Test that the form name is sent in the upload request.
TEST_F(FormStructureTest, EncodeUploadRequest_WithFormName) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_action_signature(15724779818122431245U);
  upload.set_form_name("myform");
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_FRAME_DETACHED);

  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 9U);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequestPartialMetadata) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_passwords_revealed(false);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

  test::FillUploadField(upload.add_field(), 1318412689U, nullptr, "text",
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, "lastname", "text",
                        "family-name", 5U);
  test::FillUploadField(upload.add_field(), 1545468175U, "lastname", "email",
                        "email", 9U);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

// Sending field metadata to the server is disabled.
TEST_F(FormStructureTest, EncodeUploadRequest_DisabledMetadataTrial) {
  DisableAutofillMetadataFieldTrial();

  std::unique_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  std::vector<ServerFieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.id = ASCIIToUTF16("first_name");
  field.autocomplete_attribute = "given-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.id = ASCIIToUTF16("last_name");
  field.autocomplete_attribute = "family-name";
  field.css_classes = ASCIIToUTF16("class1 class2");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.id = ASCIIToUTF16("e-mail");
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_passwords_revealed(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);

  test::FillUploadField(upload.add_field(), 3763331450U, nullptr, nullptr,
                        nullptr, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, nullptr, nullptr,
                        nullptr, 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, nullptr, nullptr,
                        nullptr, 9U);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, true, std::string(), true, &encoded_upload));

  std::string encoded_upload_string;
  encoded_upload.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

// Check that we compute the "datapresent" string correctly for the given
// |available_types|.
TEST_F(FormStructureTest, CheckDataPresence) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
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
  upload.set_form_signature(form_structure.form_signature());
  upload.set_autofill_used(false);
  upload.set_data_present("");
  upload.set_passwords_revealed(false);
  upload.set_action_signature(15724779818122431245U);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);

  test::FillUploadField(upload.add_field(), 1089846351U, "first", "text",
                        nullptr, 1U);
  test::FillUploadField(upload.add_field(), 2404144663U, "last", "text",
                        nullptr, 1U);
  test::FillUploadField(upload.add_field(), 420638584U, "email", "text",
                        nullptr, 1U);

  std::string expected_upload_string;
  ASSERT_TRUE(upload.SerializeToString(&expected_upload_string));

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload));

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
      available_field_types, false, std::string(), true, &encoded_upload2));

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
      available_field_types, false, std::string(), true, &encoded_upload3));

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
      available_field_types, false, std::string(), true, &encoded_upload4));

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
      available_field_types, false, std::string(), true, &encoded_upload5));

  encoded_upload5.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, CheckMultipleTypes) {
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
  form.origin = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last");
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
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
  upload.set_form_signature(form_structure->form_signature());
  upload.set_autofill_used(false);
  upload.set_data_present("1440000360000008");
  upload.set_passwords_revealed(false);
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

  AutofillUploadContents encoded_upload;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), true, &encoded_upload));

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
      available_field_types, false, std::string(), true, &encoded_upload2));

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
      available_field_types, false, std::string(), true, &encoded_upload3));

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
      available_field_types, false, std::string(), true, &encoded_upload4));

  encoded_upload4.SerializeToString(&encoded_upload_string);
  EXPECT_EQ(expected_upload_string, encoded_upload_string);
}

TEST_F(FormStructureTest, EncodeUploadRequest_PasswordsRevealed) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");

  // Add 3 fields, to make the form uploadable.
  FormFieldData field;
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);
  field.name = ASCIIToUTF16("last");
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_passwords_were_revealed(true);
  AutofillUploadContents upload;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(
      {{}} /* available_field_types */, false /* form_was_autofilled */,
      std::string() /* login_form_signature */, true /* observed_submission */,
      &upload));
  EXPECT_EQ(true, upload.passwords_revealed());
}

TEST_F(FormStructureTest, CheckFormSignature) {
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
  field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  form.fields.push_back(field);

  form_structure = std::make_unique<FormStructure>(form);

  EXPECT_EQ(FormStructureTest::Hash64Bit(std::string("://&&email&first")),
            form_structure->FormSignatureAsStr());

  form.origin = GURL(std::string("http://www.facebook.com"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTest::Hash64Bit(
                std::string("http://www.facebook.com&&email&first")),
            form_structure->FormSignatureAsStr());

  form.action = GURL(std::string("https://login.facebook.com/path"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTest::Hash64Bit(
                std::string("https://login.facebook.com&&email&first")),
            form_structure->FormSignatureAsStr());

  form.name = ASCIIToUTF16("login_form");
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_EQ(FormStructureTest::Hash64Bit(std::string(
                "https://login.facebook.com&login_form&email&first")),
            form_structure->FormSignatureAsStr());

  // Checks how digits are removed from field names.
  field.check_status = FormFieldData::NOT_CHECKABLE;
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
  EXPECT_EQ(FormStructureTest::Hash64Bit(
                std::string("https://login.facebook.com&login_form&email&first&"
                            "random1234&random&1ran12dom&random123")),
            form_structure->FormSignatureAsStr());
}

TEST_F(FormStructureTest, ToFormData) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

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

TEST_F(FormStructureTest, SkipFieldTest) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("select");
  field.name = ASCIIToUTF16("select");
  field.form_control_type = "checkbox";
  field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  field.check_status = FormFieldData::NOT_CHECKABLE;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<std::string> encoded_signatures;
  AutofillQueryContents encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillQueryContents query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillQueryContents::Form* query_form = query.add_form();
  query_form->set_signature(form_structure.form_signature());

  test::FillQueryField(query_form->add_field(), 239111655U, "username", "text");
  test::FillQueryField(query_form->add_field(), 420638584U, "email", "text");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  const char kExpectedSignature[] = "18006745212084723782";

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures[0]);

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

TEST_F(FormStructureTest, EncodeQueryRequest_WithLabels) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

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
  std::vector<std::string> encoded_signatures;
  AutofillQueryContents encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillQueryContents query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillQueryContents::Form* query_form = query.add_form();
  query_form->set_signature(form_structure.form_signature());

  test::FillQueryField(query_form->add_field(), 239111655U, "username", "text");
  test::FillQueryField(query_form->add_field(), 420638584U, "email", "text");
  test::FillQueryField(query_form->add_field(), 2051817934U, "password",
                       "password");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  EXPECT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query));

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

TEST_F(FormStructureTest, EncodeQueryRequest_WithLongLabels) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

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
  std::vector<std::string> encoded_signatures;
  AutofillQueryContents encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillQueryContents query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillQueryContents::Form* query_form = query.add_form();
  query_form->set_signature(form_structure.form_signature());

  test::FillQueryField(query_form->add_field(), 239111655U, "username", "text");
  test::FillQueryField(query_form->add_field(), 420638584U, "email", "text");
  test::FillQueryField(query_form->add_field(), 2051817934U, "password",
                       "password");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  EXPECT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query));

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

// One name is missing from one field.
TEST_F(FormStructureTest, EncodeQueryRequest_MissingNames) {
  FormData form;
  // No name set for the form.
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  // No name set for this field.
  field.name = ASCIIToUTF16("");
  field.form_control_type = "text";
  field.check_status = FormFieldData::NOT_CHECKABLE;
  form.fields.push_back(field);

  FormStructure form_structure(form);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<std::string> encoded_signatures;
  AutofillQueryContents encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillQueryContents query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillQueryContents::Form* query_form = query.add_form();
  query_form->set_signature(form_structure.form_signature());

  test::FillQueryField(query_form->add_field(), 239111655U, "username", "text");
  test::FillQueryField(query_form->add_field(), 1318412689U, nullptr, "text");

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  const char kExpectedSignature[] = "16416961345885087496";

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures[0]);

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

// Sending field metadata to the server is disabled.
TEST_F(FormStructureTest, EncodeQueryRequest_DisabledMetadataTrial) {
  DisableAutofillMetadataFieldTrial();

  FormData form;
  // No name set for the form.
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "text";
  field.check_status = FormFieldData::NOT_CHECKABLE;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<std::string> encoded_signatures;
  AutofillQueryContents encoded_query;

  // Create the expected query and serialize it to a string.
  AutofillQueryContents query;
  query.set_client_version("6.1.1715.1442/en (GGLL)");
  AutofillQueryContents::Form* query_form = query.add_form();
  query_form->set_signature(form_structure.form_signature());

  test::FillQueryField(query_form->add_field(), 239111655U, nullptr, nullptr);
  test::FillQueryField(query_form->add_field(), 3654076265U, nullptr, nullptr);

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  const char kExpectedSignature[] = "7635954436925888745";

  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures[0]);

  std::string encoded_query_string;
  encoded_query.SerializeToString(&encoded_query_string);
  EXPECT_EQ(expected_query_string, encoded_query_string);
}

TEST_F(FormStructureTest, PossibleValues) {
  FormData form_data;
  form_data.origin = GURL("http://www.foo.com/");

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

// Tests proper resolution heuristic, server and html field types when the
// server returns NO_SERVER_DATA, UNKNOWN_TYPE, and a valid type.
TEST_F(FormStructureTest, ParseQueryResponse_UnknownType) {
  FormData form_data;
  FormFieldData field;
  form_data.origin = GURL("http://foo.com");
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
  AutofillQueryResponseContents response;
  std::string response_string;
  response.add_field()->set_overall_type_prediction(UNKNOWN_TYPE);
  response.add_field()->set_overall_type_prediction(NO_SERVER_DATA);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_LINE1);
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Parse the response and update the field type predictions.
  std::vector<FormStructure*> forms{&form};
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);
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

// TODO(crbug.com/578257): Add more tests for the AutofillQueryResponseContents
// proto.
TEST_F(FormStructureTest, ParseQueryResponse) {
  FormData form;
  form.origin = GURL("http://foo.com");
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
  checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
  form.fields.push_back(checkable_field);

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  FormStructure form_structure2(form);
  forms.push_back(&form_structure2);

  AutofillQueryResponseContents response;
  AutofillQueryResponseContents_Field* field0 = response.add_field();
  field0->set_overall_type_prediction(7);
  AutofillQueryResponseContents_Field_FieldPrediction* field_prediction0 =
      field0->add_predictions();
  field_prediction0->set_type(7);
  AutofillQueryResponseContents_Field_FieldPrediction* field_prediction1 =
      field0->add_predictions();
  field_prediction1->set_type(22);
  response.add_field()->set_overall_type_prediction(30);
  response.add_field()->set_overall_type_prediction(9);
  response.add_field()->set_overall_type_prediction(0);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

  ASSERT_GE(forms[0]->field_count(), 2U);
  ASSERT_GE(forms[1]->field_count(), 2U);
  EXPECT_EQ(7, forms[0]->field(0)->server_type());
  ASSERT_EQ(2U, forms[0]->field(0)->server_predictions().size());
  EXPECT_EQ(7U, forms[0]->field(0)->server_predictions()[0].type());
  EXPECT_EQ(22U, forms[0]->field(0)->server_predictions()[1].type());
  EXPECT_EQ(30, forms[0]->field(1)->server_type());
  ASSERT_EQ(1U, forms[0]->field(1)->server_predictions().size());
  EXPECT_EQ(30U, forms[0]->field(1)->server_predictions()[0].type());
  EXPECT_EQ(9, forms[1]->field(0)->server_type());
  ASSERT_EQ(1U, forms[1]->field(0)->server_predictions().size());
  EXPECT_EQ(9U, forms[1]->field(0)->server_predictions()[0].type());
  EXPECT_EQ(0, forms[1]->field(1)->server_type());
  ASSERT_EQ(1U, forms[1]->field(1)->server_predictions().size());
  EXPECT_EQ(0U, forms[1]->field(1)->server_predictions()[0].type());
}

TEST_F(FormStructureTest, ParseQueryResponse_AuthorDefinedTypes) {
  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(EMAIL_ADDRESS);
  response.add_field()->set_overall_type_prediction(ACCOUNT_CREATION_PASSWORD);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

  ASSERT_GE(forms[0]->field_count(), 2U);
  // Server type is parsed from the response and is the end result type.
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(0)->server_type());
  EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ACCOUNT_CREATION_PASSWORD, forms[0]->field(1)->server_type());
  // TODO(crbug.com/613666): Should be a properly defined type, and not
  // UNKNOWN_TYPE.
  EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(1)->Type().GetStorableType());
}

TEST_F(FormStructureTest, ParseQueryResponse_RationalizeLoneField) {
  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_LINE1);
  response.add_field()->set_overall_type_prediction(
      CREDIT_CARD_EXP_MONTH);  // Uh-oh!
  response.add_field()->set_overall_type_prediction(EMAIL_ADDRESS);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Test that the expiry month field is rationalized away when enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);

    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
    ASSERT_EQ(1U, forms.size());
    ASSERT_EQ(4U, forms[0]->field_count());
    EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(2)->Type().GetStorableType());
    EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(3)->Type().GetStorableType());
  }

  // Sanity check that the enable/disabled works.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);

    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
    ASSERT_EQ(1U, forms.size());
    ASSERT_EQ(4U, forms[0]->field_count());
    EXPECT_EQ(NAME_FULL, forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(ADDRESS_HOME_LINE1, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
              forms[0]->field(2)->Type().GetStorableType());
    EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(3)->Type().GetStorableType());
  }
}

TEST_F(FormStructureTest, ParseQueryResponse_RationalizeCCName) {
  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NAME_FIRST);
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NAME_LAST);
  response.add_field()->set_overall_type_prediction(EMAIL_ADDRESS);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Test that the name fields are rationalized when enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);

    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
    ASSERT_EQ(1U, forms.size());
    ASSERT_EQ(3U, forms[0]->field_count());
    EXPECT_EQ(NAME_FIRST, forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(NAME_LAST, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(2)->Type().GetStorableType());
  }

  // Sanity check that the enable/disabled works.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);

    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
    ASSERT_EQ(1U, forms.size());
    ASSERT_EQ(3U, forms[0]->field_count());
    EXPECT_EQ(CREDIT_CARD_NAME_FIRST,
              forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_NAME_LAST,
              forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(EMAIL_ADDRESS, forms[0]->field(2)->Type().GetStorableType());
  }
}

TEST_F(FormStructureTest, ParseQueryResponse_RationalizeMultiMonth_1) {
  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NAME_FULL);
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NUMBER);
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_EXP_MONTH);
  response.add_field()->set_overall_type_prediction(
      CREDIT_CARD_EXP_2_DIGIT_YEAR);
  response.add_field()->set_overall_type_prediction(
      CREDIT_CARD_EXP_MONTH);  // Uh-oh!

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Test that the extra month field is rationalized away when enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);

    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
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

  // Sanity check that the enable/disabled works.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);

    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
    ASSERT_EQ(1U, forms.size());
    ASSERT_EQ(5U, forms[0]->field_count());
    EXPECT_EQ(CREDIT_CARD_NAME_FULL,
              forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
              forms[0]->field(2)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_2_DIGIT_YEAR,
              forms[0]->field(3)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
              forms[0]->field(4)->Type().GetStorableType());
  }
}

TEST_F(FormStructureTest, ParseQueryResponse_RationalizeMultiMonth_2) {
  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NAME_FULL);
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NUMBER);
  response.add_field()->set_overall_type_prediction(
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  response.add_field()->set_overall_type_prediction(
      CREDIT_CARD_EXP_MONTH);  // Uh-oh!

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Test that the extra month field is rationalized away when enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);
    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
    ASSERT_EQ(1U, forms.size());
    ASSERT_EQ(4U, forms[0]->field_count());
    EXPECT_EQ(CREDIT_CARD_NAME_FULL,
              forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
              forms[0]->field(2)->Type().GetStorableType());
    EXPECT_EQ(UNKNOWN_TYPE, forms[0]->field(3)->Type().GetStorableType());
  }

  // Sanity check that the enable/disabled works.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        autofill::features::kAutofillRationalizeFieldTypePredictions);
    FormStructure::ParseQueryResponse(response_string, forms, nullptr);
    ASSERT_EQ(1U, forms.size());
    ASSERT_EQ(4U, forms[0]->field_count());
    EXPECT_EQ(CREDIT_CARD_NAME_FULL,
              forms[0]->field(0)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_NUMBER, forms[0]->field(1)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
              forms[0]->field(2)->Type().GetStorableType());
    EXPECT_EQ(CREDIT_CARD_EXP_MONTH,
              forms[0]->field(3)->Type().GetStorableType());
  }
}

TEST_F(FormStructureTest, FindLongestCommonPrefix) {
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

TEST_F(FormStructureTest, RationalizePhoneNumber_RunsOncePerSection) {
  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(PHONE_HOME_WHOLE_NUMBER);
  response.add_field()->set_overall_type_prediction(PHONE_HOME_WHOLE_NUMBER);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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
TEST_F(FormStructureTest, RationalizeRepeatedFields_OneAddress) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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
TEST_F(FormStructureTest, RationalizeRepreatedFields_TwoAddresses) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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
TEST_F(FormStructureTest, RationalizeRepreatedFields_ThreeAddresses) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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
TEST_F(FormStructureTest, RationalizeRepreatedFields_FourAddresses) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);
  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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
TEST_F(FormStructureTest, RationalizeRepreatedFields_OneAddressEachSection) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  // Billing
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);
  // Shipping
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);
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
// doesn't happen in real world, bc it is in fact two sections according to
// heuristics, and is only made for testing.
TEST_F(
    FormStructureTest,
    RationalizeRepreatedFields_SectionTwoAddress_SectionThreeAddress_SectionFourAddresses) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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
TEST_F(FormStructureTest,
       RationalizeRepreatedFields_MultipleSectionsByHeuristics_OneAddressEach) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  // Billing
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);
  // Shipping
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);
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
    FormStructureTest,
    RationalizeRepreatedFields_MultipleSectionsByHeuristics_TwoAddress_ThreeAddress) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(
      ADDRESS_HOME_STREET_ADDRESS);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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

TEST_F(FormStructureTest,
       RationalizeRepreatedFields_StateCountry_NoRationalization) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  // second section
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  // third section
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  // fourth section
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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

TEST_F(FormStructureTest, RationalizeRepreatedFields_CountryStateNoHeuristics) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  // second section
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_BILLING_STATE);
  // third section
  response.add_field()->set_overall_type_prediction(ADDRESS_BILLING_STATE);
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_BILLING_STATE);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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

TEST_F(FormStructureTest,
       RationalizeRepreatedFields_StateCountryWithHeuristics) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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
  field.role = AutofillField::ROLE_ATTRIBUTE_PRESENTATION;  // hidden
  form.fields.push_back(field);

  field.role = AutofillField::ROLE_ATTRIBUTE_OTHER;  // visible

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
  field.role = AutofillField::ROLE_ATTRIBUTE_PRESENTATION;  // hidden
  form.fields.push_back(field);

  field.role = AutofillField::ROLE_ATTRIBUTE_OTHER;  // visible

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
  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  // second section
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);
  response.add_field()->set_overall_type_prediction(ADDRESS_BILLING_COUNTRY);
  // third section
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_CITY);
  response.add_field()->set_overall_type_prediction(ADDRESS_BILLING_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_BILLING_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

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

TEST_F(FormStructureTest, RationalizeRepreatedFields_FirstFieldRationalized) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_STATE);
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_BILLING_STATE);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(5U, forms[0]->field_count());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(NAME_FULL, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(4)->Type().GetStorableType());
}

TEST_F(FormStructureTest, RationalizeRepreatedFields_LastFieldRationalized) {
  base::test::ScopedFeatureList feature_list;
  InitFeature(&feature_list, kAutofillRationalizeRepeatedServerPredictions,
              true);

  FormData form;
  form.origin = GURL("http://foo.com");
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

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(NAME_FULL);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);
  response.add_field()->set_overall_type_prediction(ADDRESS_HOME_COUNTRY);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);

  // Will call RationalizeFieldTypePredictions
  FormStructure::ParseQueryResponse(response_string, forms, nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(6U, forms[0]->field_count());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, forms[0]->field(2)->Type().GetStorableType());
  EXPECT_EQ(NAME_FULL, forms[0]->field(3)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(4)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE, forms[0]->field(5)->Type().GetStorableType());
}

TEST_F(FormStructureTest, AllowBigForms) {
  FormData form;
  form.origin = GURL("http://foo.com");
  FormFieldData field;
  // Check that the form with 100 fields are processed correctly.
  for (size_t i = 0; i < 100; ++i) {
    field.form_control_type = "text";
    field.name = ASCIIToUTF16("text") + base::NumberToString16(i);
    form.fields.push_back(field);
  }

  FormStructure form_structure(form);

  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  std::vector<std::string> encoded_signatures;

  AutofillQueryContents encoded_query;
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_query));
  EXPECT_EQ(1u, encoded_signatures.size());
}

// Tests that an Autofill upload for password form with 1 field should not be
// uploaded.
TEST_F(FormStructureTest, OneFieldPasswordFormShouldNotBeUpload) {
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

}  // namespace autofill
