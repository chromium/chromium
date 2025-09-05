// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_qualifiers.h"

#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace {

class FormStructureShouldTest : public testing::Test {
 public:
  static bool ShouldBeParsed(const FormStructure& form,
                             ShouldBeParsedParams params = {}) {
    const bool r = ShouldBeParsedForTest(form, params, nullptr);
    CHECK_EQ(r, ShouldBeParsedForTest(form.ToFormData(), params, nullptr))
        << "ShouldBeParsed(FormStructure) and ShouldBeParsed(FormData) must be "
           "equivalent";
    return r;
  }

  static bool ShouldRunHeuristics(const FormStructure& form) {
    const bool r = autofill::ShouldRunHeuristics(form);
    CHECK_EQ(r, autofill::ShouldRunHeuristics(form.ToFormData()))
        << "ShouldRunHeuristics(FormStructure) and "
           "ShouldRunHeuristics(FormData) must be equivalent";
    return r;
  }

  static bool FormIsAutofillable(const FormData& form) {
    const RegexPredictions regex_predictions = DetermineRegexTypes(
        GeoIpCountryCode(""), LanguageCode(""), form, nullptr);
    FormStructure form_structure(form);
    regex_predictions.ApplyTo(form_structure.fields());
    form_structure.RationalizeAndAssignSections(GeoIpCountryCode(""),
                                                LanguageCode(""), nullptr);
    return IsAutofillable(form_structure);
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

class FormShouldBeParsedTest : public FormStructureShouldTest {
 public:
  FormShouldBeParsedTest() {
    form_.set_url(GURL("http://www.foo.com/"));
    form_structure_ = std::make_unique<FormStructure>(form_);
  }

  ~FormShouldBeParsedTest() override = default;

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
TEST_F(FormShouldBeParsedTest, FalseIfNoFields) {
  EXPECT_FALSE(ShouldBeParsed(form_structure()));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));
}

// Forms with only checkable fields should not be parsed.
TEST_F(FormShouldBeParsedTest, IgnoresCheckableFields) {
  // Start with a single checkable field.
  {
    FormFieldData field;
    field.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
    field.set_form_control_type(FormControlType::kInputRadio);
    AddField(field);
  }
  EXPECT_FALSE(ShouldBeParsed(form_structure()));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));

  // Add a second checkable field.
  {
    FormFieldData field;
    field.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
    field.set_form_control_type(FormControlType::kInputCheckbox);
    AddField(field);
  }
  EXPECT_FALSE(ShouldBeParsed(form_structure()));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));

  // Add one text field.
  AddTextField();
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));
}

// Forms with at least one text field should be parsed.
TEST_F(FormShouldBeParsedTest, TrueIfOneTextField) {
  AddTextField();
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));

  AddTextField();
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));
}

// Forms that have only select fields should not be parsed.
TEST_F(FormShouldBeParsedTest, FalseIfOnlySelectField) {
  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kSelectOne);
    AddField(field);
  }
  EXPECT_FALSE(ShouldBeParsed(form_structure()));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));

  AddTextField();
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));
}

// Form whose action is a search URL should not be parsed.
TEST_F(FormShouldBeParsedTest, FalseIfSearchURL) {
  AddTextField();
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));

  // The target cannot include http(s)://*/search...
  SetAction(GURL("http://google.com/search?q=hello"));
  EXPECT_FALSE(ShouldBeParsed(form_structure()));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));

  // But search can be in the URL.
  SetAction(GURL("http://search.com/?q=hello"));
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 1}));
}

// Forms with two password fields and no other fields should be parsed.
TEST_F(FormShouldBeParsedTest, TrueIfOnlyPasswordFields) {
  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputPassword);
    AddField(field);
  }
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(
      form_structure(),
      {.min_required_fields = 2,
       .required_fields_for_forms_with_only_password_fields = 1}));
  EXPECT_FALSE(ShouldBeParsed(
      form_structure(),
      {.min_required_fields = 2,
       .required_fields_for_forms_with_only_password_fields = 2}));

  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputPassword);
    AddField(field);
  }
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(
      form_structure(),
      {.min_required_fields = 2,
       .required_fields_for_forms_with_only_password_fields = 1}));
  EXPECT_TRUE(ShouldBeParsed(
      form_structure(),
      {.min_required_fields = 2,
       .required_fields_for_forms_with_only_password_fields = 2}));
}

// Forms with at least one field with an autocomplete attribute should be
// parsed.
TEST_F(FormShouldBeParsedTest, TrueIfOneFieldHasAutocomplete) {
  AddTextField();
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));
  EXPECT_FALSE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));

  {
    FormFieldData field;
    field.set_parsed_autocomplete(AutocompleteParsingResult{
        .section = "my-billing-section", .field_type = HtmlFieldType::kName});
    field.set_form_control_type(FormControlType::kInputText);
    AddField(field);
  }
  EXPECT_TRUE(ShouldBeParsed(form_structure()));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));
  EXPECT_TRUE(ShouldBeParsed(form_structure(), {.min_required_fields = 2}));
}

TEST_F(FormStructureShouldTest, ShouldBeParsed_BadScheme) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_fields(
      {test::CreateTestFormField("Name", "name", "",
                                 FormControlType::kInputText, "name"),
       test::CreateTestFormField("Email", "email", "",
                                 FormControlType::kInputText, "email"),
       test::CreateTestFormField("Address", "address", "",
                                 FormControlType::kInputText,
                                 "address-line1")});

  // Baseline, HTTP should work.
  form.set_url(GURL("http://wwww.foo.com/myform"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(ShouldBeParsed(*form_structure));
  EXPECT_TRUE(ShouldRunHeuristics(*form_structure));
  EXPECT_TRUE(ShouldBeQueried(*form_structure));
  EXPECT_TRUE(ShouldBeUploaded(*form_structure));

  // Baseline, HTTPS should work.
  form.set_url(GURL("https://wwww.foo.com/myform"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(ShouldBeParsed(*form_structure));
  EXPECT_TRUE(ShouldRunHeuristics(*form_structure));
  EXPECT_TRUE(ShouldBeQueried(*form_structure));
  EXPECT_TRUE(ShouldBeUploaded(*form_structure));

  // Chrome internal urls shouldn't be parsed.
  form.set_url(GURL("chrome://settings"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(ShouldBeParsed(*form_structure));
  EXPECT_FALSE(ShouldRunHeuristics(*form_structure));
  EXPECT_FALSE(ShouldBeQueried(*form_structure));
  EXPECT_FALSE(ShouldBeUploaded(*form_structure));

  // FTP urls shouldn't be parsed.
  form.set_url(GURL("ftp://ftp.foo.com/form.html"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(ShouldBeParsed(*form_structure));
  EXPECT_FALSE(ShouldRunHeuristics(*form_structure));
  EXPECT_FALSE(ShouldBeQueried(*form_structure));
  EXPECT_FALSE(ShouldBeUploaded(*form_structure));

  // Blob urls shouldn't be parsed.
  form.set_url(GURL("blob://blob.foo.com/form.html"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(ShouldBeParsed(*form_structure));
  EXPECT_FALSE(ShouldRunHeuristics(*form_structure));
  EXPECT_FALSE(ShouldBeQueried(*form_structure));
  EXPECT_FALSE(ShouldBeUploaded(*form_structure));

  // About urls shouldn't be parsed.
  form.set_url(GURL("about://about.foo.com/form.html"));
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_FALSE(ShouldBeParsed(*form_structure));
  EXPECT_FALSE(ShouldRunHeuristics(*form_structure));
  EXPECT_FALSE(ShouldBeQueried(*form_structure));
  EXPECT_FALSE(ShouldBeUploaded(*form_structure));
}

// Tests that ShouldBeParsed returns true for a form containing less than three
// fields if at least one has an autocomplete attribute.
TEST_F(FormStructureShouldTest, ShouldBeParsed_TwoFields_HasAutocomplete) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_fields({test::CreateTestFormField(
                       "Name", "name", "", FormControlType::kInputText, "name"),
                   test::CreateTestFormField("Address", "Address", "",
                                             FormControlType::kSelectOne, "")});
  form_structure = std::make_unique<FormStructure>(form);
  EXPECT_TRUE(ShouldBeParsed(*form_structure));
}

TEST_F(FormStructureShouldTest, IsAutofillable) {
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

}  // namespace
}  // namespace autofill
