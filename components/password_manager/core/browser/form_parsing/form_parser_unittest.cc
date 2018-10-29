// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_parser.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

using UsernameDetectionMethod = FormDataParser::UsernameDetectionMethod;

// Use this value in FieldDataDescription.value to get an arbitrary unique value
// generated in GetFormDataAndExpectation().
constexpr char kNonimportantValue[] = "non-important unique";

// Use this in FieldDataDescription below to mark the expected username and
// password fields. The *_FILLING variants apply to
// FormDataParser::Mode::kFilling only, the *_SAVING variants to
// FormDataParser::Mode::kSaving only, the suffix-less variants to both.
enum class ElementRole {
  NONE,
  USERNAME_FILLING,
  USERNAME_SAVING,
  USERNAME,
  CURRENT_PASSWORD_FILLING,
  CURRENT_PASSWORD_SAVING,
  CURRENT_PASSWORD,
  NEW_PASSWORD_FILLING,
  NEW_PASSWORD_SAVING,
  NEW_PASSWORD,
  CONFIRMATION_PASSWORD_FILLING,
  CONFIRMATION_PASSWORD_SAVING,
  CONFIRMATION_PASSWORD
};

// Expected FormFieldData are constructed based on these descriptions.
struct FieldDataDescription {
  ElementRole role = ElementRole::NONE;
  bool is_focusable = true;
  bool is_enabled = true;
  bool is_readonly = false;
  autofill::FieldPropertiesMask properties_mask =
      FieldPropertiesFlags::NO_FLAGS;
  const char* autocomplete_attribute = nullptr;
  const char* value = kNonimportantValue;
  const char* name = kNonimportantValue;
  const char* form_control_type = "text";
  PasswordFieldPrediction prediction = {.type = autofill::MAX_VALID_FIELD_TYPE};
  // If not -1, indicates on which rank among predicted usernames this should
  // be. Unused ranks will be padded with unique IDs (not found in any fields).
  int predicted_username = -1;
};

// Describes a test case for the parser.
struct FormParsingTestCase {
  const char* description_for_logging;
  std::vector<FieldDataDescription> fields;
  // -1 just mean no checking.
  int number_of_all_possible_passwords = -1;
  int number_of_all_possible_usernames = -1;
  // null means no checking
  const autofill::ValueElementVector* all_possible_passwords = nullptr;
  const autofill::ValueElementVector* all_possible_usernames = nullptr;
  bool username_may_use_prefilled_placeholder = false;
  base::Optional<FormDataParser::ReadonlyPasswordFields> readonly_status;
};

// Returns numbers which are distinct from each other within the scope of one
// test.
uint32_t GetUniqueId() {
  static uint32_t counter = 10;
  return counter++;
}

// Use to add a number suffix which is unique in the scope of the test.
base::string16 StampUniqueSuffix(const char* base_str) {
  return ASCIIToUTF16(base_str) + ASCIIToUTF16("_") +
         base::UintToString16(GetUniqueId());
}

// Describes which renderer IDs are expected for username/password fields
// identified in a PasswordForm.
struct ParseResultIds {
  uint32_t username_id = FormFieldData::kNotSetFormControlRendererId;
  uint32_t password_id = FormFieldData::kNotSetFormControlRendererId;
  uint32_t new_password_id = FormFieldData::kNotSetFormControlRendererId;
  uint32_t confirmation_password_id =
      FormFieldData::kNotSetFormControlRendererId;

  bool IsEmpty() const {
    return username_id == FormFieldData::kNotSetFormControlRendererId &&
           password_id == FormFieldData::kNotSetFormControlRendererId &&
           new_password_id == FormFieldData::kNotSetFormControlRendererId &&
           confirmation_password_id ==
               FormFieldData::kNotSetFormControlRendererId;
  }
};

// Creates a FormData to be fed to the parser. Includes FormFieldData as
// described in |fields_description|. Generates |fill_result| and |save_result|
// expectations about the result in FILLING and SAVING mode, respectively. Also
// fills |predictions| with the predictions contained in FieldDataDescriptions.
FormData GetFormDataAndExpectation(
    const std::vector<FieldDataDescription>& fields_description,
    FormPredictions* predictions,
    ParseResultIds* fill_result,
    ParseResultIds* save_result) {
  FormData form_data;
  form_data.action = GURL("http://example1.com");
  form_data.origin = GURL("http://example2.com");
  for (const FieldDataDescription& field_description : fields_description) {
    FormFieldData field;
    const uint32_t unique_id = GetUniqueId();
    field.unique_renderer_id = unique_id;
    field.id = StampUniqueSuffix("html_id");
    if (field_description.name == kNonimportantValue) {
      field.name = StampUniqueSuffix("html_name");
    } else {
      field.name = ASCIIToUTF16(field_description.name);
    }
    field.form_control_type = field_description.form_control_type;
    field.is_focusable = field_description.is_focusable;
    field.is_enabled = field_description.is_enabled;
    field.is_readonly = field_description.is_readonly;
    field.properties_mask = field_description.properties_mask;
    if (field_description.value == kNonimportantValue) {
      field.value = StampUniqueSuffix("value");
    } else {
      field.value = ASCIIToUTF16(field_description.value);
    }
    if (field_description.autocomplete_attribute)
      field.autocomplete_attribute = field_description.autocomplete_attribute;
    form_data.fields.push_back(field);
    switch (field_description.role) {
      case ElementRole::NONE:
        break;
      case ElementRole::USERNAME_FILLING:
        fill_result->username_id = unique_id;
        break;
      case ElementRole::USERNAME_SAVING:
        save_result->username_id = unique_id;
        break;
      case ElementRole::USERNAME:
        fill_result->username_id = unique_id;
        save_result->username_id = unique_id;
        break;
      case ElementRole::CURRENT_PASSWORD_FILLING:
        fill_result->password_id = unique_id;
        break;
      case ElementRole::CURRENT_PASSWORD_SAVING:
        save_result->password_id = unique_id;
        break;
      case ElementRole::CURRENT_PASSWORD:
        fill_result->password_id = unique_id;
        save_result->password_id = unique_id;
        break;
      case ElementRole::NEW_PASSWORD_FILLING:
        fill_result->new_password_id = unique_id;
        break;
      case ElementRole::NEW_PASSWORD_SAVING:
        save_result->new_password_id = unique_id;
        break;
      case ElementRole::NEW_PASSWORD:
        fill_result->new_password_id = unique_id;
        save_result->new_password_id = unique_id;
        break;
      case ElementRole::CONFIRMATION_PASSWORD_FILLING:
        fill_result->confirmation_password_id = unique_id;
        break;
      case ElementRole::CONFIRMATION_PASSWORD_SAVING:
        save_result->confirmation_password_id = unique_id;
        break;
      case ElementRole::CONFIRMATION_PASSWORD:
        fill_result->confirmation_password_id = unique_id;
        save_result->confirmation_password_id = unique_id;
        break;
    }
    if (field_description.prediction.type != autofill::MAX_VALID_FIELD_TYPE) {
      (*predictions)[unique_id] = field_description.prediction;
    }
    if (field_description.predicted_username >= 0) {
      size_t index = static_cast<size_t>(field_description.predicted_username);
      if (form_data.username_predictions.size() <= index)
        form_data.username_predictions.resize(index + 1);
      form_data.username_predictions[index] = field.unique_renderer_id;
    }
  }
  // Fill unused ranks in predictions with fresh IDs to check that those are
  // correctly ignored. In real situation, this might correspond, e.g., to
  // fields which were not fillable and hence dropped from the selection.
  for (uint32_t& id : form_data.username_predictions) {
    if (id == 0)
      id = GetUniqueId();
  }
  return form_data;
}

// Check that |fields| has a field with unique renderer ID |renderer_id| which
// has the name |element_name| and value |*element_value|. If |renderer_id| is
// FormFieldData::kNotSetFormControlRendererId, then instead check that
// |element_name| and |*element_value| are empty. Set |element_kind| to identify
// the type of the field in logging: 'username', 'password', etc. The argument
// |element_value| can be null, in which case all checks involving it are
// skipped (useful for the confirmation password value, which is not represented
// in PasswordForm).
void CheckField(const std::vector<FormFieldData>& fields,
                uint32_t renderer_id,
                const base::string16& element_name,
                const base::string16* element_value,
                const char* element_kind) {
  SCOPED_TRACE(testing::Message("Looking for element of kind ")
               << element_kind);

  if (renderer_id == FormFieldData::kNotSetFormControlRendererId) {
    EXPECT_EQ(base::string16(), element_name);
    if (element_value)
      EXPECT_EQ(base::string16(), *element_value);
    return;
  }

  auto field_it = std::find_if(fields.begin(), fields.end(),
                               [renderer_id](const FormFieldData& field) {
                                 return field.unique_renderer_id == renderer_id;
                               });
  ASSERT_TRUE(field_it != fields.end())
      << "Could not find a field with renderer ID " << renderer_id;

// On iOS |id| is used for identifying DOM elements, so the parser should return
// it.
#if defined(OS_IOS)
  EXPECT_EQ(element_name, field_it->id);
#else
  EXPECT_EQ(element_name, field_it->name);
#endif

  if (element_value)
    EXPECT_EQ(*element_value, field_it->value);
}

// Check that the information distilled from |form_data| into |password_form| is
// matching |expectations|.
void CheckPasswordFormFields(const PasswordForm& password_form,
                             const FormData& form_data,
                             const ParseResultIds& expectations) {
  CheckField(form_data.fields, expectations.username_id,
             password_form.username_element, &password_form.username_value,
             "username");
  EXPECT_EQ(expectations.username_id,
            password_form.username_element_renderer_id);

  CheckField(form_data.fields, expectations.password_id,
             password_form.password_element, &password_form.password_value,
             "password");
  EXPECT_EQ(expectations.password_id,
            password_form.password_element_renderer_id);

  CheckField(form_data.fields, expectations.new_password_id,
             password_form.new_password_element,
             &password_form.new_password_value, "new_password");

  CheckField(form_data.fields, expectations.confirmation_password_id,
             password_form.confirmation_password_element, nullptr,
             "confirmation_password");
}

// Checks that in a vector of pairs of string16s, all the first parts of the
// pairs (which represent element values) are unique.
void CheckAllValuesUnique(const autofill::ValueElementVector& v) {
  std::set<base::string16> all_values;
  for (const auto pair : v) {
    auto insertion = all_values.insert(pair.first);
    EXPECT_TRUE(insertion.second) << pair.first << " is duplicated";
  }
}

// Iterates over |test_cases|, creates a FormData for each, runs the parser and
// checks the results.
void CheckTestData(const std::vector<FormParsingTestCase>& test_cases) {
  for (const FormParsingTestCase& test_case : test_cases) {
    FormPredictions predictions;
    ParseResultIds fill_result;
    ParseResultIds save_result;
    const FormData form_data = GetFormDataAndExpectation(
        test_case.fields, &predictions, &fill_result, &save_result);
    FormDataParser parser;
    parser.set_predictions(std::move(predictions));
    for (auto mode :
         {FormDataParser::Mode::kFilling, FormDataParser::Mode::kSaving}) {
      SCOPED_TRACE(
          testing::Message("Test description: ")
          << test_case.description_for_logging << ", parsing mode = "
          << (mode == FormDataParser::Mode::kFilling ? "Filling" : "Saving"));

      std::unique_ptr<PasswordForm> parsed_form = parser.Parse(form_data, mode);

      const ParseResultIds& expected_ids =
          mode == FormDataParser::Mode::kFilling ? fill_result : save_result;

      if (expected_ids.IsEmpty()) {
        EXPECT_FALSE(parsed_form) << "Expected no parsed results";
      } else {
        ASSERT_TRUE(parsed_form) << "Expected successful parsing";
        EXPECT_EQ(PasswordForm::SCHEME_HTML, parsed_form->scheme);
        EXPECT_FALSE(parsed_form->preferred);
        EXPECT_FALSE(parsed_form->blacklisted_by_user);
        EXPECT_EQ(PasswordForm::TYPE_MANUAL, parsed_form->type);
        EXPECT_TRUE(parsed_form->has_renderer_ids);
        EXPECT_EQ(test_case.username_may_use_prefilled_placeholder,
                  parsed_form->username_may_use_prefilled_placeholder);
        CheckPasswordFormFields(*parsed_form, form_data, expected_ids);
        CheckAllValuesUnique(parsed_form->all_possible_passwords);
        CheckAllValuesUnique(parsed_form->other_possible_usernames);
        if (test_case.number_of_all_possible_passwords >= 0) {
          EXPECT_EQ(
              static_cast<size_t>(test_case.number_of_all_possible_passwords),
              parsed_form->all_possible_passwords.size());
        }
        if (test_case.all_possible_passwords) {
          EXPECT_EQ(*test_case.all_possible_passwords,
                    parsed_form->all_possible_passwords);
        }
        if (test_case.number_of_all_possible_usernames >= 0) {
          EXPECT_EQ(
              static_cast<size_t>(test_case.number_of_all_possible_usernames),
              parsed_form->other_possible_usernames.size());
        }
        if (test_case.all_possible_usernames) {
          EXPECT_EQ(*test_case.all_possible_usernames,
                    parsed_form->other_possible_usernames);
        }
      }
      if (test_case.readonly_status) {
        EXPECT_EQ(*test_case.readonly_status, parser.readonly_status());
      }
    }
  }
}

TEST(FormParserTest, NotPasswordForm) {
  CheckTestData({
      {
          "No fields", {},
      },
      {
          .description_for_logging = "No password fields",
          .fields =
              {
                  {.form_control_type = "text"}, {.form_control_type = "text"},
              },
          .number_of_all_possible_passwords = 0,
          .number_of_all_possible_usernames = 0,
      },
  });
}

TEST(FormParserTest, SkipNotTextFields) {
  CheckTestData({
      {
          "A 'select' between username and password fields",
          {
              {.role = ElementRole::USERNAME},
              {.form_control_type = "select"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
          .number_of_all_possible_passwords = 1,
          .number_of_all_possible_usernames = 1,
      },
  });
}

TEST(FormParserTest, OnlyPasswordFields) {
  CheckTestData({
      {
          .description_for_logging = "1 password field",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 1,
          .number_of_all_possible_usernames = 0,
      },
      {
          "2 password fields, new and confirmation password",
          {
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
          },
      },
      {
          "2 password fields, current and new password",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
          },
      },
      {
          "3 password fields, current, new, confirm password",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
          },
      },
      {
          .description_for_logging = "3 password fields with different values",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .value = "pw1"},
                  {.form_control_type = "password", .value = "pw2"},
                  {.form_control_type = "password", .value = "pw3"},
              },
          .number_of_all_possible_passwords = 3,
      },
      {
          "4 password fields, only the first 3 are considered",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.form_control_type = "password", .value = "pw3"},
          },
      },
      {
          "4 password fields, 4th same value as 3rd and 2nd, only the first 3 "
          "are considered",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.form_control_type = "password", .value = "pw2"},
          },
      },
      {
          "4 password fields, all same value",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
              {.form_control_type = "password", .value = "pw"},
              {.form_control_type = "password", .value = "pw"},
              {.form_control_type = "password", .value = "pw"},
          },
      },
  });
}

TEST(FormParserTest, TestFocusability) {
  CheckTestData({
      {
          "non-focusable fields are considered when there are no focusable "
          "fields",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false},
          },
      },
      {
          "non-focusable should be skipped when there are focusable fields",
          {
              {.form_control_type = "password", .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
          },
      },
      {
          "non-focusable text fields before password",
          {
              {.form_control_type = "text", .is_focusable = false},
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
          },
          .number_of_all_possible_usernames = 2,
      },
      {
          "focusable and non-focusable text fields before password",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_focusable = true},
              {.form_control_type = "text", .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
          },
      },
      {
          .description_for_logging = "many passwords, some of them focusable",
          .fields =
              {
                  {.form_control_type = "password", .is_focusable = false},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .is_focusable = true},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .is_focusable = true,
                   .value = "pw"},
                  {.form_control_type = "password", .is_focusable = false},
                  {.form_control_type = "password", .is_focusable = false},
                  {.form_control_type = "password", .is_focusable = false},
                  {.form_control_type = "password", .is_focusable = false},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = "password",
                   .is_focusable = true,
                   .value = "pw"},
                  {.form_control_type = "password", .is_focusable = false},
                  {.form_control_type = "password", .is_focusable = false},
              },
          // 9 distinct values in 10 password fields:
          .number_of_all_possible_passwords = 9,
      },
  });
}

TEST(FormParserTest, TextAndPasswordFields) {
  CheckTestData({
      {
          "Simple empty sign-in form",
          // Forms with empty fields cannot be saved, so the parsing result for
          // saving is empty.
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
          // all_possible_* only count fields with non-empty values.
          .number_of_all_possible_passwords = 0,
          .number_of_all_possible_usernames = 0,
      },
      {
          .description_for_logging = "Simple sign-in form with filled data",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 1,
      },
      {
          "Empty sign-in form with an extra text field",
          {
              {.form_control_type = "text", .value = ""},
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
      },
      {
          "Non-empty sign-in form with an extra text field",
          {
              {.role = ElementRole::USERNAME_SAVING,
               .form_control_type = "text"},
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Empty sign-in form with an extra invisible text field",
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.form_control_type = "text", .is_focusable = false, .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
      },
      {
          "Non-empty sign-in form with an extra invisible text field",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.form_control_type = "text", .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Simple empty sign-in form with empty username",
          // Filled forms with a username field which is left empty are
          // suspicious. The parser will just omit the username altogether.
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Simple empty sign-in form with empty password",
          // Empty password, nothing to save.
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
      },
  });
}

TEST(FormParserTest, TestAutocomplete) {
  CheckTestData({
      {
          .description_for_logging =
              "All possible password autocomplete attributes and some fields "
              "without autocomplete",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .autocomplete_attribute = "username"},
                  {.form_control_type = "text"},
                  {.form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "current-password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "new-password",
                   .value = "np"},
                  {.form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "new-password",
                   .value = "np"},
              },
          // 4 distinct password values in 5 password fields
          .number_of_all_possible_passwords = 4,
      },
      {
          .description_for_logging =
              "Non-password autocomplete attributes are skipped",
          .fields =
              {
                  {.form_control_type = "text",
                   .autocomplete_attribute = "email"},
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .value = "pw"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = "password",
                   .value = "pw"},
                  // NB: 'password' is not a valid autocomplete type hint.
                  {.form_control_type = "password",
                   .autocomplete_attribute = "password"},
              },
          .number_of_all_possible_passwords = 3,
          .number_of_all_possible_usernames = 2,
      },
      {
          "Basic heuristics kick in if autocomplete analysis fails",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "email"},
              // NB: 'password' is not a valid autocomplete type hint.
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Partial autocomplete analysis fails if no passwords are found",
          // The attribute 'username' is ignored, because there was no password
          // marked up.
          {
              {.form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Multiple username autocomplete attributes, fallback to base "
          "heuristics",
          {
              {.form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "current-password"},
          },
      },
      {
          "Parsing complex autocomplete attributes",
          {
              // Valid information about form sections, in addition to the
              // username hint.
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "section-test billing username"},
              {.form_control_type = "text"},
              // Invalid composition, but the parser is simplistic and just
              // grabs the last token.
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "new-password current-password"},
              {.form_control_type = "password"},
          },
      },
      {
          "Ignored autocomplete attributes",
          {
              // 'off' is ignored.
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "off"},
              // Invalid composition, the parser ignores all but the last token.
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "new-password abc"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Swapped username/password autocomplete attributes",
          // Swap means ignoring autocomplete analysis and falling back to basic
          // heuristics.
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "current-password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "username"},
          },
      },
      {
          "Autocomplete mark-up overrides visibility",
          {
              {.role = ElementRole::USERNAME,
               .is_focusable = false,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.is_focusable = true, .form_control_type = "text"},
              {.is_focusable = true, .form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false,
               .autocomplete_attribute = "current-password"},
          },
      },
  });
}

TEST(FormParserTest, DisabledFields) {
  CheckTestData({
      {
          .description_for_logging = "The disabled attribute is ignored",
          .fields =
              {
                  {.is_enabled = true, .form_control_type = "text"},
                  {.role = ElementRole::USERNAME,
                   .is_enabled = false,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .is_enabled = false},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .is_enabled = true},
              },
          .number_of_all_possible_passwords = 2,
      },
  });
}

TEST(FormParserTest, SkippingFieldsWithCreditCardFields) {
  CheckTestData({
      {
          "Simple form, all fields are credit-card-related",
          {
              {.form_control_type = "text",
               .autocomplete_attribute = "cc-name"},
              {.form_control_type = "password",
               .autocomplete_attribute = "cc-any-string"},
          },
      },
      {
          .description_for_logging = "Non-CC fields are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.form_control_type = "text",
                   .autocomplete_attribute = "cc-name"},
                  {.form_control_type = "password",
                   .autocomplete_attribute = "cc-any-string"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 2,
      },
  });
}

TEST(FormParserTest, ReadonlyFields) {
  CheckTestData({
      {
          "For usernames, readonly does not matter",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_readonly = true},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "For passwords, readonly means: 'give up', perhaps there is a "
          "virtual keyboard, filling might be ignored",
          {
              {.form_control_type = "text"},
              {.form_control_type = "password", .is_readonly = true},
          },
      },
      {
          "But correctly marked passwords are accepted even if readonly",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::NEW_PASSWORD,
               .autocomplete_attribute = "new-password",
               .form_control_type = "password",
               .is_readonly = true},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .autocomplete_attribute = "new-password",
               .form_control_type = "password",
               .is_readonly = true},
              {.role = ElementRole::CURRENT_PASSWORD,
               .autocomplete_attribute = "current-password",
               .form_control_type = "password",
               .is_readonly = true},
          },
      },
      {
          .description_for_logging = "And passwords already filled by user or "
                                     "Chrome on pageload are accepted even if "
                                     "readonly",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .properties_mask =
                       FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD,
                   .form_control_type = "password",
                   .is_readonly = true},
                  {.role = ElementRole::NEW_PASSWORD,
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .form_control_type = "password",
                   .is_readonly = true},
                  {.form_control_type = "password", .is_readonly = true},
              },
          .number_of_all_possible_passwords = 3,
      },
      {
          .description_for_logging = "And passwords already filled by user or "
                                     "Chrome with FOAS are accepted even if "
                                     "readonly",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .properties_mask =
                       FieldPropertiesFlags::AUTOFILLED_ON_USER_TRIGGER,
                   .form_control_type = "password",
                   .is_readonly = true},
                  {.role = ElementRole::NEW_PASSWORD,
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .form_control_type = "password",
                   .is_readonly = true},
                  {.form_control_type = "password", .is_readonly = true},
              },
          .number_of_all_possible_passwords = 3,
      },
  });
}

TEST(FormParserTest, ServerHints) {
  CheckTestData({
      {
          "Empty predictions don't cause panic",
          {
              {.form_control_type = "text"},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Username-only predictions are ignored",
          {
              {.form_control_type = "text",
               .prediction = {.type = autofill::USERNAME,
                              .may_use_prefilled_placeholder = true}},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Simple predictions work",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .prediction = {.type = autofill::USERNAME_AND_EMAIL_ADDRESS,
                              .may_use_prefilled_placeholder = true}},
              {.form_control_type = "text"},
              {.form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .prediction = {.type = autofill::PASSWORD,
                              .may_use_prefilled_placeholder = true},
               .form_control_type = "password"},
          },
          .username_may_use_prefilled_placeholder = true,
      },
      {
          .description_for_logging = "Longer predictions work",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .prediction = {.type = autofill::USERNAME},
                   .form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD},
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .prediction = {.type = autofill::CONFIRMATION_PASSWORD},
                   .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .prediction = {.type = autofill::PASSWORD},
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 4,
      },
  });
}

TEST(FormParserTest, Interactability) {
  CheckTestData({
      {
          "If all fields are hidden, all are considered",
          {
              {.form_control_type = "text", .is_focusable = false},
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false},
          },
      },
      {
          .description_for_logging =
              "If some fields are hidden, only visible are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .is_focusable = true},
                  {.form_control_type = "text", .is_focusable = false},
                  {.form_control_type = "password", .is_focusable = false},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .is_focusable = true},
              },
          .number_of_all_possible_passwords = 2,
      },
      {
          .description_for_logging =
              "If user typed somewhere, only typed-into fields are considered, "
              "even if not currently visible",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .form_control_type = "text",
                   .is_focusable = false},
                  {.form_control_type = "text", .is_focusable = true},
                  {.form_control_type = "password", .is_focusable = false},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .properties_mask = FieldPropertiesFlags::AUTOFILLED,
                   .is_focusable = true},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .is_focusable = true},
              },
          .number_of_all_possible_passwords = 3,
      },
      {
          "Interactability for usernames is only considered before the first "
          "relevant password. That way, if, e.g., the username gets filled and "
          "hidden (to let the user enter password), and there is another text "
          "field visible below, the maximum Interactability won't end up being "
          "kPossible, which would exclude the hidden username.",
          {
              {.role = ElementRole::USERNAME,
               .properties_mask = FieldPropertiesFlags::AUTOFILLED,
               .form_control_type = "text",
               .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .properties_mask = FieldPropertiesFlags::AUTOFILLED,
               .is_focusable = true},
              {.form_control_type = "text", .is_focusable = true, .value = ""},
          },
      },
      {
          "Interactability also matters for HTML classifier.",
          {
              {.form_control_type = "text",
               .is_focusable = false,
               .predicted_username = 0},
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_focusable = true},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
          },
      },
  });
}

TEST(FormParserTest, AllPossiblePasswords) {
  const autofill::ValueElementVector kPasswords = {
      {ASCIIToUTF16("a"), ASCIIToUTF16("p1")},
      {ASCIIToUTF16("b"), ASCIIToUTF16("p3")},
  };
  const autofill::ValueElementVector kUsernames = {
      {ASCIIToUTF16("b"), ASCIIToUTF16("chosen")},
      {ASCIIToUTF16("a"), ASCIIToUTF16("first")},
  };
  CheckTestData({
      {
          .description_for_logging = "It is always the first field name which "
                                     "is associated with a duplicated password "
                                     "value",
          .fields =
              {
                  {.form_control_type = "password", .name = "p1", .value = "a"},
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .name = "chosen",
                   .value = "b",
                   .autocomplete_attribute = "username"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "current-password",
                   .value = "a"},
                  {.form_control_type = "text", .name = "first", .value = "a"},
                  {.form_control_type = "text", .value = "a"},
                  {.form_control_type = "password", .name = "p3", .value = "b"},
                  {.form_control_type = "password", .value = "b"},
              },
          .number_of_all_possible_passwords = 2,
          .all_possible_passwords = &kPasswords,
          .number_of_all_possible_usernames = 2,
          .all_possible_usernames = &kUsernames,
      },
      {
          .description_for_logging =
              "Empty values don't get added to all_possible_passwords",
          .fields =
              {
                  {.form_control_type = "password", .value = ""},
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .autocomplete_attribute = "username"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "current-password",
                   .value = ""},
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.form_control_type = "password", .value = ""},
                  {.form_control_type = "password", .value = ""},
              },
          .number_of_all_possible_passwords = 0,
      },
      {
          .description_for_logging =
              "A particular type of a squashed form (sign-in + sign-up)",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .autocomplete_attribute = "username"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "current-password"},
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.form_control_type = "password"},
                  {.form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 3,
      },
      {
          .description_for_logging = "A strange but not squashed form",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.form_control_type = "password"},
                  {.form_control_type = "password"},
                  {.form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 4,
      },
  });
}

TEST(FormParserTest, UsernamePredictions) {
  CheckTestData({
      {
          "Username prediction overrides structure",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .predicted_username = 0},
              {.form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Username prediction does not override structure if empty and mode "
          "is SAVING",
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .predicted_username = 2,
               .value = ""},
              {.role = ElementRole::USERNAME_SAVING,
               .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Username prediction does not override autocomplete analysis",
          {
              {.form_control_type = "text", .predicted_username = 0},
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "current-password"},
          },
      },
      {
          "Username prediction does not override server hints",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .prediction = {.type = autofill::USERNAME_AND_EMAIL_ADDRESS}},
              {.form_control_type = "text", .predicted_username = 0},
              {.role = ElementRole::CURRENT_PASSWORD,
               .prediction = {.type = autofill::PASSWORD},
               .form_control_type = "password"},
          },
      },
      {
          "Username prediction order matters",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .predicted_username = 1},
              {.form_control_type = "text", .predicted_username = 4},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
  });
}

// In some situations, server hints or autocomplete mark-up do not provide the
// username might be omitted. Sometimes this is a truthful signal (there might
// be no username despite the presence of plain text fields), but often this is
// just incomplete data. In the long term, the server hints should be complete
// and also cover cases when the autocomplete mark-up is lacking; at that point,
// the parser should just trust that the signal is truthful. Until then,
// however, the parser is trying to complement the signal with its structural
// heuristics.
TEST(FormParserTest, ComplementingResults) {
  CheckTestData({
      {
          "Current password from autocomplete analysis, username from basic "
          "heuristics",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "current-password"},
          },
      },
      {
          "New and confirmation passwords from server, username from basic "
          "heuristics",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .prediction = {.type = autofill::CONFIRMATION_PASSWORD},
               .form_control_type = "password"},
              {.form_control_type = "text"},
              {.role = ElementRole::NEW_PASSWORD,
               .prediction = {.type = autofill::NEW_PASSWORD},
               .form_control_type = "password"},
          },
      },
      {
          "No password from server still means that serve hints are ignored.",
          {
              {.prediction = {.type = autofill::USERNAME_AND_EMAIL_ADDRESS},
               .form_control_type = "text"},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
  });
}

// Until autofill server learns to provide CVC-related hints, the parser should
// try to get the hint from the field names.
TEST(FormParserTest, CVC) {
  CheckTestData({
      {
          "Name of 'verification_type' matches the CVC pattern.",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.form_control_type = "text", .name = "verification_type"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
  });
}

// Check that "readonly status" is reported accordingly.
TEST(FormParserTest, ReadonlyStatus) {
  CheckTestData({
      {
          "Server hints prevent heuristics from using readonly.",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .prediction = {.type = autofill::PASSWORD},
               .is_readonly = true,
               .form_control_type = "password"},
          },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
      },
      {
          "Autocomplete attributes prevent heuristics from using readonly.",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .autocomplete_attribute = "current-password",
               .is_readonly = true,
               .form_control_type = "password"},
          },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
      },
      {
          "No password fields are a special case of not going through local "
          "heuristics.",
          {
              {.form_control_type = "text"},
          },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
      },
      {
          "No readonly passwords ignored.",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               // While readonly, this field is not ignored because it was
               // autofilled before.
               .is_readonly = true,
               .properties_mask = FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD,
               .form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .is_readonly = false,
               .form_control_type = "password"},
          },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoneIgnored,
      },
      {
          "Some readonly passwords ignored.",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.is_readonly = true, .form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .is_readonly = false,
               .form_control_type = "password"},
          },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kSomeIgnored,
      },
      {
          "All readonly passwords ignored.",
          {
              {.form_control_type = "text"},
              {.is_readonly = true, .form_control_type = "password"},
          },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kAllIgnored,
      },
  });
}

TEST(FormParserTest, HistogramsForUsernameDetectionMethod) {
  struct HistogramTestCase {
    FormParsingTestCase parsing_data;
    UsernameDetectionMethod expected_method;
  } kHistogramTestCases[] = {
      {
          {
              "No username",
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
          },
          UsernameDetectionMethod::kNoUsernameDetected,
      },
      {
          {
              "Reporting server analysis",
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
          },
          UsernameDetectionMethod::kServerSidePrediction,
      },
      {
          {
              "Reporting autocomplete analysis",
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .autocomplete_attribute = "username"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "current-password"},
              },
          },
          UsernameDetectionMethod::kAutocompleteAttribute,
      },
      {
          {
              "Reporting HTML classifier",
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .predicted_username = 0},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          },
          UsernameDetectionMethod::kHtmlBasedClassifier,
      },
      {
          {
              "Reporting basic heuristics",
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          },
          UsernameDetectionMethod::kBaseHeuristic,
      },
      {
          {
              "Mixing server analysis on password and HTML classifier on "
              "username is reported as HTML classifier",
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .predicted_username = 0},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
          },
          UsernameDetectionMethod::kHtmlBasedClassifier,
      },
      {
          {
              "Mixing autocomplete analysis on password and basic heuristics "
              "on username is reported as basic heuristics",
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .autocomplete_attribute = "current-password"},
              },
          },
          UsernameDetectionMethod::kBaseHeuristic,
      },
  };
  for (const HistogramTestCase& histogram_test_case : kHistogramTestCases) {
    base::HistogramTester tester;
    CheckTestData({histogram_test_case.parsing_data});
    // Expect two samples, because parsing is done once for filling and once for
    // saving mode.
    SCOPED_TRACE(histogram_test_case.parsing_data.description_for_logging);
    tester.ExpectUniqueSample("PasswordManager.UsernameDetectionMethod",
                              histogram_test_case.expected_method,
                              2);
  }
}

TEST(FormParserTest, GetSignonRealm) {
  struct TestCase {
    const char* input;
    const char* expected_output;
  } test_cases[]{
      {"http://example.com/", "http://example.com/"},
      {"http://example.com/signup", "http://example.com/"},
      {"https://google.com/auth?a=1#b", "https://google.com/"},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message("Input: ")
                 << test_case.input << " "
                 << "Expected output: " << test_case.expected_output);
    GURL input(test_case.input);
    EXPECT_EQ(test_case.expected_output, GetSignonRealm(input));
  }
}

}  // namespace

}  // namespace password_manager
