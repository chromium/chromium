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
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using autofill::mojom::SubmissionIndicatorEvent;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

using UsernameDetectionMethod = FormDataParser::UsernameDetectionMethod;

// Use this value in FieldDataDescription.value to get an arbitrary unique value
// generated in GetFormDataAndExpectation().
constexpr char kNonimportantValue[] = "non-important unique";

// Use this in FieldDataDescription below to mark the expected username and
// password fields.
enum class ElementRole {
  NONE,
  USERNAME,
  CURRENT_PASSWORD,
  NEW_PASSWORD,
  CONFIRMATION_PASSWORD
};

// Expected FormFieldData are constructed based on these descriptions.
struct FieldDataDescription {
  // The |role*| fields state the expected role of the field. The
  // |role_filling| speaks specifically about parsing in
  // FormDataParser::Mode::kFilling only, the |role_saving| about
  // FormDataParser::Mode::kSaving. If set, |role| overrides both of the
  // others.
  ElementRole role = ElementRole::NONE;
  ElementRole role_filling = ElementRole::NONE;
  ElementRole role_saving = ElementRole::NONE;
  bool is_focusable = true;
  bool is_enabled = true;
  bool is_readonly = false;
  autofill::FieldPropertiesMask properties_mask =
      FieldPropertiesFlags::NO_FLAGS;
  const char* autocomplete_attribute = nullptr;
  const char* value = kNonimportantValue;
  const char* typed_value = nullptr;
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
  base::Optional<FormDataParser::ReadonlyPasswordFields>
      readonly_status_for_saving;
  base::Optional<FormDataParser::ReadonlyPasswordFields>
      readonly_status_for_filling;
  // If the result should be marked as only useful for fallbacks.
  bool fallback_only = false;
  SubmissionIndicatorEvent submission_event = SubmissionIndicatorEvent::NONE;
  base::Optional<bool> is_new_password_reliable;
  bool form_has_autofilled_value = false;
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
         base::NumberToString16(GetUniqueId());
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

// Updates |result| by putting |id| in the appropriate |result|'s field based
// on |role|.
void UpdateResultWithIdByRole(ParseResultIds* result,
                              uint32_t id,
                              ElementRole role) {
  constexpr uint32_t kUnassigned = FormFieldData::kNotSetFormControlRendererId;
  switch (role) {
    case ElementRole::NONE:
      // Nothing to update.
      break;
    case ElementRole::USERNAME:
      DCHECK_EQ(kUnassigned, result->username_id);
      result->username_id = id;
      break;
    case ElementRole::CURRENT_PASSWORD:
      DCHECK_EQ(kUnassigned, result->password_id);
      result->password_id = id;
      break;
    case ElementRole::NEW_PASSWORD:
      DCHECK_EQ(kUnassigned, result->new_password_id);
      result->new_password_id = id;
      break;
    case ElementRole::CONFIRMATION_PASSWORD:
      DCHECK_EQ(kUnassigned, result->confirmation_password_id);
      result->confirmation_password_id = id;
      break;
  }
}

// Creates a FormData to be fed to the parser. Includes FormFieldData as
// described in |fields_description|. Generates |fill_result| and |save_result|
// expectations about the result in FILLING and SAVING mode, respectively. Also
// fills |predictions| with the predictions contained in FieldDataDescriptions.
FormData GetFormDataAndExpectation(const FormParsingTestCase& test_case,
                                   FormPredictions* predictions,
                                   ParseResultIds* fill_result,
                                   ParseResultIds* save_result) {
  FormData form_data;
  form_data.action = GURL("http://example1.com");
  form_data.url = GURL("http://example2.com");
  form_data.submission_event = test_case.submission_event;
  for (const FieldDataDescription& field_description : test_case.fields) {
    FormFieldData field;
    const uint32_t renderer_id = GetUniqueId();
    field.unique_renderer_id = renderer_id;
    field.id_attribute = StampUniqueSuffix("html_id");
    if (field_description.name == kNonimportantValue) {
      field.name = StampUniqueSuffix("html_name");
    } else {
      field.name = ASCIIToUTF16(field_description.name);
    }
    field.name_attribute = field.name;
#if defined(OS_IOS)
    field.unique_id = StampUniqueSuffix("unique_id");
#endif
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
    if (field_description.typed_value)
      field.typed_value = ASCIIToUTF16(field_description.typed_value);
    form_data.fields.push_back(field);
    if (field_description.role == ElementRole::NONE) {
      UpdateResultWithIdByRole(fill_result, renderer_id,
                               field_description.role_filling);
      UpdateResultWithIdByRole(save_result, renderer_id,
                               field_description.role_saving);
    } else {
      UpdateResultWithIdByRole(fill_result, renderer_id,
                               field_description.role);
      UpdateResultWithIdByRole(save_result, renderer_id,
                               field_description.role);
    }
    if (field_description.prediction.type != autofill::MAX_VALID_FIELD_TYPE) {
      predictions->fields.push_back(field_description.prediction);
      predictions->fields.back().renderer_id = renderer_id;
#if defined(OS_IOS)
      predictions->fields.back().unique_id = field.unique_id;
#endif
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

// On iOS |unique_id| is used for identifying DOM elements, so the parser should
// return it. See crbug.com/896594
#if defined(OS_IOS)
  EXPECT_EQ(element_name, field_it->unique_id);
#else
  EXPECT_EQ(element_name, field_it->name);
#endif

  base::string16 expected_value =
      field_it->typed_value.empty() ? field_it->value : field_it->typed_value;

  if (element_value)
    EXPECT_EQ(expected_value, *element_value);
}

// Describes the |form_data| including field values and names. Use this in
// SCOPED_TRACE if other logging messages might refer to the form.
testing::Message DescribeFormData(const FormData& form_data) {
  testing::Message result;
  result << "Form contains " << form_data.fields.size() << " fields:\n";
  for (const FormFieldData& field : form_data.fields) {
    result << "type=" << field.form_control_type << ", name=" << field.name
           << ", value=" << field.value
           << ", unique id=" << field.unique_renderer_id << "\n";
  }
  return result;
}

// Check that the information distilled from |form_data| into |password_form| is
// matching |expectations|.
void CheckPasswordFormFields(const PasswordForm& password_form,
                             const FormData& form_data,
                             const ParseResultIds& expectations) {
  SCOPED_TRACE(DescribeFormData(form_data));
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
        test_case, &predictions, &fill_result, &save_result);
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
        EXPECT_EQ(PasswordForm::Scheme::kHtml, parsed_form->scheme);
        EXPECT_FALSE(parsed_form->preferred);
        EXPECT_FALSE(parsed_form->blacklisted_by_user);
        EXPECT_EQ(PasswordForm::Type::kManual, parsed_form->type);
#if defined(OS_IOS)
        EXPECT_FALSE(parsed_form->has_renderer_ids);
#else
        EXPECT_TRUE(parsed_form->has_renderer_ids);
#endif
        EXPECT_EQ(test_case.username_may_use_prefilled_placeholder,
                  parsed_form->username_may_use_prefilled_placeholder);
        EXPECT_EQ(test_case.submission_event, parsed_form->submission_event);
        if (test_case.is_new_password_reliable &&
            mode == FormDataParser::Mode::kFilling) {
          EXPECT_EQ(*test_case.is_new_password_reliable,
                    parsed_form->is_new_password_reliable);
        }
        EXPECT_EQ(test_case.form_has_autofilled_value,
                  parsed_form->form_has_autofilled_value);

        CheckPasswordFormFields(*parsed_form, form_data, expected_ids);
        CheckAllValuesUnique(parsed_form->all_possible_passwords);
        CheckAllValuesUnique(parsed_form->all_possible_usernames);
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
              parsed_form->all_possible_usernames.size());
        }
        if (test_case.all_possible_usernames) {
          EXPECT_EQ(*test_case.all_possible_usernames,
                    parsed_form->all_possible_usernames);
        }
        if (mode == FormDataParser::Mode::kSaving) {
          EXPECT_EQ(test_case.fallback_only, parsed_form->only_for_fallback);
        }
      }
      if (test_case.readonly_status) {
        EXPECT_EQ(*test_case.readonly_status, parser.readonly_status());
      } else {
        const base::Optional<FormDataParser::ReadonlyPasswordFields>*
            expected_readonly_status =
                mode == FormDataParser::Mode::kSaving
                    ? &test_case.readonly_status_for_saving
                    : &test_case.readonly_status_for_filling;
        if (expected_readonly_status->has_value())
          EXPECT_EQ(*expected_readonly_status, parser.readonly_status());
      }
    }
  }
}

TEST(FormParserTest, NotPasswordForm) {
  CheckTestData({
      {
          .description_for_logging = "No fields",
          .fields = {},
      },
      {
          .description_for_logging = "No password fields",
          .fields =
              {
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
              },
          .number_of_all_possible_passwords = 0,
          .number_of_all_possible_usernames = 0,
      },
  });
}

TEST(FormParserTest, SkipNotTextFields) {
  CheckTestData({
      {
          .description_for_logging =
              "A 'select' between username and password fields",
          .fields =
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
          .description_for_logging =
              "2 password fields, new and confirmation password",
          .fields =
              {
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = "pw",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = "pw",
                   .form_control_type = "password"},
              },
          .is_new_password_reliable = false,
      },
      {
          .description_for_logging =
              "2 password fields, current and new password",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = "pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = "pw2",
                   .form_control_type = "password"},
              },
          .is_new_password_reliable = false,
      },
      {
          .description_for_logging =
              "3 password fields, current, new, confirm password",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = "pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = "pw2",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = "pw2",
                   .form_control_type = "password"},
              },
          .is_new_password_reliable = false,
      },
      {
          .description_for_logging = "3 password fields with different values",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = "pw1",
                   .form_control_type = "password"},
                  {.value = "pw2", .form_control_type = "password"},
                  {.value = "pw3", .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 3,
      },
      {
          .description_for_logging =
              "4 password fields, only the first 3 are considered",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = "pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = "pw2",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = "pw2",
                   .form_control_type = "password"},
                  {.value = "pw3", .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "4 password fields, 4th same value as 3rd "
                                     "and 2nd, only the first 3 "
                                     "are considered",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = "pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = "pw2",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = "pw2",
                   .form_control_type = "password"},
                  {.value = "pw2", .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "4 password fields, all same value",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = "pw",
                   .form_control_type = "password"},
                  {.value = "pw", .form_control_type = "password"},
                  {.value = "pw", .form_control_type = "password"},
                  {.value = "pw", .form_control_type = "password"},
              },
      },
  });
}

TEST(FormParserTest, TestFocusability) {
  CheckTestData({
      {
          .description_for_logging =
              "non-focusable fields are considered when there are no focusable "
              "fields",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "non-focusable should be skipped when there are focusable fields",
          .fields =
              {
                  {.is_focusable = false, .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "non-focusable text fields before password",
          .fields =
              {
                  {.is_focusable = false, .form_control_type = "text"},
                  {.role = ElementRole::USERNAME,
                   .is_focusable = false,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = "password"},
              },
          .number_of_all_possible_usernames = 2,
      },
      {
          .description_for_logging =
              "focusable and non-focusable text fields before password",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_focusable = true,
                   .form_control_type = "text"},
                  {.is_focusable = false, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "many passwords, some of them focusable",
          .fields =
              {
                  {.is_focusable = false, .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = true,
                   .value = "pw",
                   .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .is_focusable = true,
                   .value = "pw",
                   .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
              },
          // 9 distinct values in 10 password fields:
          .number_of_all_possible_passwords = 9,
      },
  });
}

TEST(FormParserTest, TextAndPasswordFields) {
  CheckTestData({
      {
          .description_for_logging = "Simple empty sign-in form",
          // Forms with empty fields cannot be saved, so the parsing result for
          // saving is empty.
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = "",
                   .form_control_type = "password"},
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
          .description_for_logging =
              "Empty sign-in form with an extra text field",
          .fields =
              {
                  {.value = "", .form_control_type = "text"},
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = "",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Non-empty sign-in form with an extra text field",
          .fields =
              {
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Empty sign-in form with an extra invisible text field",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text"},
                  {.is_focusable = false,
                   .value = "",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = "",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Non-empty sign-in form with an extra invisible text field",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.is_focusable = false, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Simple empty sign-in form with empty username",
          // Filled forms with a username field which is left empty are
          // suspicious. The parser will just omit the username altogether.
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Simple empty sign-in form with empty password",
          // Empty password, nothing to save.
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = "",
                   .form_control_type = "password"},
              },
      },
  });
}

TEST(FormParserTest, TextFieldValueIsNotUsername) {
  CheckTestData({{
      .description_for_logging = "Text field value is unlikely username so it "
                                 "should be ignored on saving",
      .fields =
          {
              {.role_filling = ElementRole::USERNAME,
               .value = "12",
               .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .value = "strong_pw",
               .form_control_type = "password"},
          },
  }});
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
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = "np",
                   .form_control_type = "password"},
                  {.form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = "np",
                   .form_control_type = "password"},
              },
          // 4 distinct password values in 5 password fields
          .number_of_all_possible_passwords = 4,
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "Non-password autocomplete attributes are skipped",
          .fields =
              {
                  {.autocomplete_attribute = "email",
                   .form_control_type = "text"},
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = "pw",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = "pw",
                   .form_control_type = "password"},
                  // NB: 'password' is not a valid autocomplete type hint.
                  {.autocomplete_attribute = "password",
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 3,
          .number_of_all_possible_usernames = 2,
      },
      {
          .description_for_logging =
              "Basic heuristics kick in if autocomplete analysis fails",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "email",
                   .form_control_type = "text"},
                  // NB: 'password' is not a valid autocomplete type hint.
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "password",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Partial autocomplete analysis doesn't fail if no passwords are "
              "found",
          // The attribute 'username' is used even if there was no password
          // marked up.
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Multiple username autocomplete attributes, fallback to base "
              "heuristics",
          .fields =
              {
                  {.autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "Parsing complex autocomplete attributes",
          .fields =
              {
                  // Valid information about form sections, in addition to the
                  // username hint.
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "section-test billing username",
                   .form_control_type = "text"},
                  {.form_control_type = "text"},
                  // Valid information about form sections, in addition to the
                  // username hint.
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "new-password current-password",
                   .form_control_type = "password"},
                  {.form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "Ignored autocomplete attributes",
          .fields =
              {
                  // 'off' is ignored.
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "off",
                   .form_control_type = "text"},
                  // Invalid composition, the parser ignores all but the last
                  // token.
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "new-password abc",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Swapped username/password autocomplete attributes",
          // Swap means ignoring autocomplete analysis and falling back to basic
          // heuristics.
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "username",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Autocomplete mark-up overrides visibility",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_focusable = false,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.is_focusable = true, .form_control_type = "text"},
                  {.is_focusable = true, .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = false,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
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
                   .is_enabled = false,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_enabled = true,
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 2,
      },
  });
}

TEST(FormParserTest, SkippingFieldsWithCreditCardFields) {
  CheckTestData({
      {
          .description_for_logging =
              "Simple form, all fields are credit-card-related",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "cc-name",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "cc-any-string",
                   .form_control_type = "password"},
              },
          .fallback_only = true,
      },
      {
          .description_for_logging = "Non-CC fields are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.autocomplete_attribute = "cc-name",
                   .form_control_type = "text"},
                  {.autocomplete_attribute = "cc-any-string",
                   .form_control_type = "password"},
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
          .description_for_logging = "For usernames, readonly does not matter",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_readonly = true,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "For passwords, readonly means: 'give up', perhaps there is a "
              "virtual keyboard, filling might be ignored",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .form_control_type = "password"},
              },
          // And "give-up" means "fallback-only".
          .fallback_only = true,
      },
      {
          .description_for_logging =
              "But correctly marked passwords are accepted even if readonly",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "new-password",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "new-password",
                   .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging = "And passwords already filled by user or "
                                     "Chrome on pageload are accepted even if "
                                     "readonly",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .properties_mask =
                       FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .form_control_type = "password"},
                  {.is_readonly = true, .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 3,
          .form_has_autofilled_value = true,
      },
      {
          .description_for_logging = "And passwords already filled by user or "
                                     "Chrome with FOAS are accepted even if "
                                     "readonly",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .properties_mask =
                       FieldPropertiesFlags::AUTOFILLED_ON_USER_TRIGGER,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .form_control_type = "password"},
                  {.is_readonly = true, .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 3,
          .form_has_autofilled_value = true,
      },
  });
}

TEST(FormParserTest, ServerPredictionsForClearTextPasswordFields) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::KEnablePasswordGenerationForClearTextFields);
  CheckTestData({
      {
          .description_for_logging = "Server prediction for account change "
                                     "password and username field.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type =
                                      autofill::USERNAME_AND_EMAIL_ADDRESS}},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::NEW_PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account change password field only.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::NEW_PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account password and username field.",
          .fields =
              {
                  {.form_control_type = "text",
                   .prediction = {.type =
                                      autofill::USERNAME_AND_EMAIL_ADDRESS}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account password field only.",
          .fields =
              {
                  {.form_control_type = "text"},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::PASSWORD}},
              },
      },
      {
          .description_for_logging = "Server prediction for account creation "
                                     "password and username field.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type =
                                      autofill::USERNAME_AND_EMAIL_ADDRESS}},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account creation password field only.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
      },
  });
}

TEST(FormParserTest, ServerHintsForDisabledPrefilledPlaceholderFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kEnableOverwritingPlaceholderUsernames);
  CheckTestData({
      {
          .description_for_logging = "Simple predictions work",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME_AND_EMAIL_ADDRESS,
                                  .may_use_prefilled_placeholder = true}},
                  {.form_control_type = "text"},
                  {.role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .role_saving = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD,
                                  .may_use_prefilled_placeholder = true}},
              },
          .username_may_use_prefilled_placeholder = false,
      },
  });
}

TEST(FormParserTest, ServerHints) {
  CheckTestData({
      {
          .description_for_logging = "Empty predictions don't cause panic",
          .fields =
              {
                  {.form_control_type = "text"},
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Username-only predictions are not ignored",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "Simple predictions work",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME_AND_EMAIL_ADDRESS,
                                  .may_use_prefilled_placeholder = true}},
                  {.form_control_type = "text"},
                  {.role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .role_saving = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD,
                                  .may_use_prefilled_placeholder = true}},
              },
          .username_may_use_prefilled_placeholder = true,
      },
      {
          .description_for_logging = "Longer predictions work",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text"},
                  {.role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
                  {.role_filling = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::CONFIRMATION_PASSWORD}},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
          .number_of_all_possible_passwords = 4,
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "password prediction for a non-password field is ignored",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::PASSWORD}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
  });
}

TEST(FormParserTest, Interactability) {
  CheckTestData({
      {
          .description_for_logging =
              "If all fields are hidden, all are considered",
          .fields =
              {
                  {.is_focusable = false, .form_control_type = "text"},
                  {.role = ElementRole::USERNAME,
                   .is_focusable = false,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "If some fields are hidden, only visible are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_focusable = true,
                   .form_control_type = "text"},
                  {.is_focusable = false, .form_control_type = "text"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = "password"},
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
                   .is_focusable = false,
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .form_control_type = "text"},
                  {.is_focusable = true, .form_control_type = "text"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::AUTOFILLED,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::USER_TYPED,
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 3,
          .form_has_autofilled_value = true,
      },
      {
          .description_for_logging =
              "Interactability for usernames is only considered before the "
              "first relevant password. That way, if, e.g., the username gets "
              "filled and hidden (to let the user enter password), and there "
              "is another text field visible below, the maximum "
              "Interactability won't end up being kPossible, which would "
              "exclude the hidden username.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_focusable = false,
                   .properties_mask = FieldPropertiesFlags::AUTOFILLED,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::AUTOFILLED,
                   .form_control_type = "password"},
                  {.is_focusable = true,
                   .value = "",
                   .form_control_type = "text"},
              },
          .form_has_autofilled_value = true,
      },
      {
          .description_for_logging =
              "Interactability also matters for HTML classifier.",
          .fields =
              {
                  {.is_focusable = false,
                   .form_control_type = "text",
                   .predicted_username = 0},
                  {.role = ElementRole::USERNAME,
                   .is_focusable = true,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = "password"},
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
                  {.value = "a", .name = "p1", .form_control_type = "password"},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = "b",
                   .name = "chosen",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = "a",
                   .form_control_type = "password"},
                  {.value = "a", .name = "first", .form_control_type = "text"},
                  {.value = "a", .form_control_type = "text"},
                  {.value = "b", .name = "p3", .form_control_type = "password"},
                  {.value = "b", .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 2,
          .number_of_all_possible_usernames = 2,
          .all_possible_passwords = &kPasswords,
          .all_possible_usernames = &kUsernames,
      },
      {
          .description_for_logging =
              "Empty values don't get added to all_possible_passwords",
          .fields =
              {
                  {.value = "", .form_control_type = "password"},
                  {.role_filling = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = "",
                   .form_control_type = "password"},
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.value = "", .form_control_type = "password"},
                  {.value = "", .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 0,
      },
      {
          .description_for_logging = "Empty values don't get added to "
                                     "all_possible_passwords even if form gets "
                                     "parsed",
          .fields =
              {
                  {.value = "", .form_control_type = "password"},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.value = "", .form_control_type = "password"},
                  {.value = "", .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 1,
      },
      {
          .description_for_logging =
              "A particular type of a squashed form (sign-in + sign-up)",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
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
          .description_for_logging = "Username prediction overrides structure",
          .fields =
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
          .description_for_logging = "Username prediction does not override "
                                     "structure if empty and mode "
                                     "is SAVING",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text",
                   .predicted_username = 2},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Username prediction does not override autocomplete analysis",
          .fields =
              {
                  {.form_control_type = "text", .predicted_username = 0},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "Username prediction does not override server hints",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type =
                                      autofill::USERNAME_AND_EMAIL_ADDRESS}},
                  {.form_control_type = "text", .predicted_username = 0},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
      },
      {
          .description_for_logging = "Username prediction order matters",
          .fields =
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
          .description_for_logging = "Current password from autocomplete "
                                     "analysis, username from basic "
                                     "heuristics",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging =
              "New and confirmation passwords from server, username from basic "
              "heuristics",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role_filling = ElementRole::CONFIRMATION_PASSWORD,
                   .role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::CONFIRMATION_PASSWORD}},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::NEW_PASSWORD}},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging = "No password from server still means. "
                                     "Username hint from server is "
                                     "used.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type =
                                      autofill::USERNAME_AND_EMAIL_ADDRESS}},
                  {.form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
      },
  });
}

// The parser should avoid identifying CVC fields as passwords.
TEST(FormParserTest, CVC) {
  CheckTestData({
      {
          .description_for_logging =
              "Server hints: CREDIT_CARD_VERIFICATION_CODE.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.form_control_type = "password",
                   .prediction = {.type =
                                      autofill::CREDIT_CARD_VERIFICATION_CODE}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          // The result should be trusted for more than just fallback, because
          // the chosen password was not a suspected CVC.
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: CREDIT_CARD_VERIFICATION_CODE on only password.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type =
                                      autofill::CREDIT_CARD_VERIFICATION_CODE}},
              },
          .fallback_only = true,
      },
      {
          .description_for_logging = "Name of 'verification_type' matches the "
                                     "CVC pattern, ignore that "
                                     "one.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.name = "verification_type",
                   .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          // The result should be trusted for more than just fallback, because
          // the chosen password was not a suspected CVC.
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Create a fallback for the only password being a CVC field.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .name = "verification_type",
                   .form_control_type = "password"},
              },
          .fallback_only = true,
      },
  });
}

// The parser should avoid identifying NOT_PASSWORD fields as passwords.
TEST(FormParserTest, NotPasswordField) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_PASSWORD.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.form_control_type = "password",
                   .prediction = {.type = autofill::NOT_PASSWORD}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_PASSWORD on only password.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::NOT_PASSWORD}},
              },
          .fallback_only = true,
      },
  });
}

// The parser should avoid identifying NOT_USERNAME fields as usernames.
TEST(FormParserTest, NotUsernameField) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_USERNAME.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::NONE,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::NOT_USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_USERNAME on only username.",
          .fields =
              {
                  {.role = ElementRole::NONE,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::NOT_USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .fallback_only = false,
      },
  });
}

// The parser should avoid identifying NOT_USERNAME fields as usernames despite
// autocomplete attribute.
TEST(FormParserTest, NotUsernameFieldDespiteAutocompelteAtrribute) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_USERNAME.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.autocomplete_attribute = "username",
                   .form_control_type = "text",
                   .prediction = {.type = autofill::NOT_USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_USERNAME on only username.",
          .fields =
              {
                  {.role = ElementRole::NONE,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text",
                   .prediction = {.type = autofill::NOT_USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .fallback_only = false,
      },
  });
}

// The parser should avoid identifying NOT_PASSWORD fields as passwords.
TEST(FormParserTest, NotPasswordFieldDespiteAutocompleteAttribute) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_PASSWORD.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.autocomplete_attribute = "current-password",
                   .form_control_type = "password",
                   .prediction = {.type = autofill::NOT_PASSWORD}},
                  {.autocomplete_attribute = "new-password",
                   .form_control_type = "password",
                   .prediction = {.type = autofill::NOT_PASSWORD}},
                  {.autocomplete_attribute = "password",
                   .form_control_type = "password",
                   .prediction = {.type = autofill::NOT_PASSWORD}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_PASSWORD on only password.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::NOT_PASSWORD}},
              },
          .fallback_only = true,
      },
  });
}

// Check that "readonly status" is reported accordingly.
TEST(FormParserTest, ReadonlyStatus) {
  CheckTestData({
      {
          .description_for_logging =
              "Server predictions are ignored in saving mode",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
          .readonly_status_for_saving =
              FormDataParser::ReadonlyPasswordFields::kAllIgnored,
          .readonly_status_for_filling =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
          .fallback_only = true,
      },
      {
          .description_for_logging =
              "Autocomplete attributes prevent heuristics from using readonly.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
      },
      {
          .description_for_logging = "No password fields are a special case of "
                                     "not going through local "
                                     "heuristics.",
          .fields =
              {
                  {.form_control_type = "text"},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
      },
      {
          .description_for_logging = "No readonly passwords ignored.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .role_saving = ElementRole::CURRENT_PASSWORD,
                   // While readonly, this field is not ignored because it was
                   // autofilled before.
                   .is_readonly = true,
                   .properties_mask =
                       FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .role_saving = ElementRole::NEW_PASSWORD,
                   .is_readonly = false,
                   .form_control_type = "password"},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoneIgnored,
          .form_has_autofilled_value = true,
      },
      {
          .description_for_logging = "Some readonly passwords ignored.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.is_readonly = true, .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = false,
                   .form_control_type = "password"},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kSomeIgnored,
          // The result should be trusted for more than just fallback, because
          // the chosen password was not readonly.
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "All readonly passwords ignored, only returned as a fallback.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .form_control_type = "password"},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kAllIgnored,
          .fallback_only = true,
      },
  });
}

// Check that empty values are ignored when parsing for saving.
TEST(FormParserTest, NoEmptyValues) {
  CheckTestData({
      {
          .description_for_logging =
              "Server hints overridden for non-empty values.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .value = "",
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "Autocomplete attributes overridden for non-empty values.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = "",
                   .form_control_type = "text"},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = "",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .form_control_type = "password"},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "Structure heuristics overridden for non-empty values.",
          .fields =
              {
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::USERNAME,
                   .value = "",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .value = "",
                   .form_control_type = "password"},
              },
      },
  });
}

// Check that multiple usernames in server hints are handled properly.
TEST(FormParserTest, MultipleUsernames) {
  CheckTestData({
      {
          .description_for_logging = "More than two usernames are ignored.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "No current password -> ignore additional usernames.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "2 current passwods -> ignore additional usernames.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
                  {.role_saving = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "No new password -> ignore additional usernames.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
      },
      {
          .description_for_logging = "Two usernames in sign-in, sign-up order.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging = "Two usernames in sign-up, sign-in order.",
          .fields =
              {
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
                  {.form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "Two usernames in sign-in, sign-up order; sign-in is pre-filled.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .properties_mask =
                       FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
      },
  });
}

// If multiple hints for new-password fields are given (e.g., because of more
// fields having the same signature), the first one should be marked as
// new-password. That way the generation can be offered before the user has
// thought of and typed their new password elsewhere. See
// https://crbug.com/902700 for more details.
TEST(FormParserTest, MultipleNewPasswords) {
  CheckTestData({
      {
          .description_for_logging = "Only one new-password recognised.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
                  {.role_saving = ElementRole::NEW_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
              },
      },
      {
          .description_for_logging =
              "Only one new-password recognised, confirmation unaffected.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
                  {.form_control_type = "password",
                   .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD}},
                  {.role_filling = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::CONFIRMATION_PASSWORD}},
              },
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
              .description_for_logging = "No username",
              .fields =
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
              .description_for_logging = "Reporting server analysis",
              .fields =
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
              .description_for_logging = "Reporting autocomplete analysis",
              .fields =
                  {
                      {.role = ElementRole::USERNAME,
                       .autocomplete_attribute = "username",
                       .form_control_type = "text"},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .autocomplete_attribute = "current-password",
                       .form_control_type = "password"},
                  },
          },
          UsernameDetectionMethod::kAutocompleteAttribute,
      },
      {
          {
              .description_for_logging = "Reporting HTML classifier",
              .fields =
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
              .description_for_logging = "Reporting basic heuristics",
              .fields =
                  {
                      {.role = ElementRole::USERNAME,
                       .form_control_type = "text"},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .form_control_type = "password"},
                  },
          },
          UsernameDetectionMethod::kBaseHeuristic,
      },
      {
          {
              .description_for_logging =
                  "Mixing server analysis on password and HTML classifier "
                  "on "
                  "username is reported as HTML classifier",
              .fields =
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
              .description_for_logging =
                  "Mixing autocomplete analysis on password and basic "
                  "heuristics "
                  "on username is reported as basic heuristics",
              .fields =
                  {
                      {.role = ElementRole::USERNAME,
                       .form_control_type = "text"},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .autocomplete_attribute = "current-password",
                       .form_control_type = "password"},
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
                              histogram_test_case.expected_method, 2);
  }
}

TEST(FormParserTest, SubmissionEvent) {
  CheckTestData({
      {.description_for_logging = "Sign-in form, submission event is not None",
       .fields =
           {
               {.role = ElementRole::USERNAME, .form_control_type = "text"},
               {.role = ElementRole::CURRENT_PASSWORD,
                .form_control_type = "password"},
           },
       .submission_event = SubmissionIndicatorEvent::XHR_SUCCEEDED},
  });
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

TEST(FormParserTest, TypedValues) {
  CheckTestData({{
      .description_for_logging = "Simple sign-in forms with typed values",
      // Tests that typed values are taken as username, password and
      // new password instead of values that are set by JavaScript.
      .fields =
          {
              {.role = ElementRole::USERNAME,
               .autocomplete_attribute = "username",
               .value = "js_username",
               .typed_value = "typed_username",
               .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .autocomplete_attribute = "current-password",
               .value = "js_password",
               .typed_value = "typed_password",
               .form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .autocomplete_attribute = "new-password",
               .value = "js_new_password",
               .typed_value = "typed_new_password",
               .form_control_type = "password"},
          },
  }});
}

TEST(FormParserTest, ContradictingPasswordPredictionAndAutocomplete) {
  CheckTestData({{
      .description_for_logging =
          "Server data and autocomplete contradics each other",
      // On saving, server predictions for passwords are ignored.
      // So autocomplete attributes define the role. On filling,
      // both server predictions and autocomplete are considered and
      // server predictions have higher priority and therefore
      // define the role. An autofill attributes cannot override it.
      .fields =
          {
              {.role_filling = ElementRole::CURRENT_PASSWORD,
               .role_saving = ElementRole::NEW_PASSWORD,
               .autocomplete_attribute = "new-password",
               .form_control_type = "password",
               .prediction = {.type = autofill::PASSWORD}},
          },
  }});
}

TEST(FormParserTest, SingleUsernamePrediction) {
  CheckTestData({
      {
          .description_for_logging = "1 field",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::SINGLE_USERNAME}},
              },
      },
      {
          .description_for_logging = "Password field is ignored",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::SINGLE_USERNAME}},
                  {.form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD}},
              },
      },
  });
}

}  // namespace

}  // namespace password_manager
