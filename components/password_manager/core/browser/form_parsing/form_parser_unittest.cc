// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_parser.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::mojom::SubmissionIndicatorEvent;

namespace password_manager {

namespace {

using UsernameDetectionMethod = FormDataParser::UsernameDetectionMethod;

// Use this value in FieldDataDescription.value to get an arbitrary unique value
// generated in GetFormDataAndExpectation().
constexpr char16_t kNonimportantValue[] = u"non-important unique";

// Use this in FieldDataDescription below to mark the expected username and
// password fields.
enum class ElementRole {
  NONE,
  USERNAME,
  CURRENT_PASSWORD,
  NEW_PASSWORD,
  CONFIRMATION_PASSWORD,
  // Used for fields tagged only for webauthn autocomplete.
  WEBAUTHN,
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
      FieldPropertiesFlags::kNoFlags;
  const char* autocomplete_attribute = nullptr;
  const std::u16string value = kNonimportantValue;
  const std::u16string user_input = u"";
  const base::StringPiece16 name = kNonimportantValue;
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
  raw_ptr<const ValueElementVector> all_possible_passwords = nullptr;
  raw_ptr<const ValueElementVector> all_possible_usernames = nullptr;
  bool server_side_classification_successful = true;
  bool username_may_use_prefilled_placeholder = false;
  absl::optional<FormDataParser::ReadonlyPasswordFields> readonly_status;
  absl::optional<FormDataParser::ReadonlyPasswordFields>
      readonly_status_for_saving;
  absl::optional<FormDataParser::ReadonlyPasswordFields>
      readonly_status_for_filling;
  // If the result should be marked as only useful for fallbacks.
  bool fallback_only = false;
  SubmissionIndicatorEvent submission_event = SubmissionIndicatorEvent::NONE;
  absl::optional<bool> is_new_password_reliable;
  bool form_has_autofilled_value = false;
  bool accepts_webauthn_credentials = false;
};

// Returns numbers which are distinct from each other within the scope of one
// test.
autofill::FieldRendererId GetUniqueId() {
  static uint32_t counter = 10;
  return autofill::FieldRendererId(counter++);
}

// Use to add a number suffix which is unique in the scope of the test.
std::u16string StampUniqueSuffix(const char16_t* base_str) {
  return base_str + std::u16string(u"_") +
         base::NumberToString16(GetUniqueId().value());
}

// Describes which renderer IDs are expected for username/password fields
// identified in a PasswordForm.
struct ParseResultIds {
  autofill::FieldRendererId username_id;
  autofill::FieldRendererId password_id;
  autofill::FieldRendererId new_password_id;
  autofill::FieldRendererId confirmation_password_id;
  std::vector<autofill::FieldRendererId> webauthn_ids;

  bool IsEmpty() const {
    return username_id.is_null() && password_id.is_null() &&
           new_password_id.is_null() && confirmation_password_id.is_null() &&
           webauthn_ids.empty();
  }
};

// Updates |result| by putting |id| in the appropriate |result|'s field based
// on |role|.
void UpdateResultWithIdByRole(ParseResultIds* result,
                              autofill::FieldRendererId id,
                              ElementRole role) {
  switch (role) {
    case ElementRole::NONE:
      // Nothing to update.
      break;
    case ElementRole::USERNAME:
      DCHECK(result->username_id.is_null());
      result->username_id = id;
      break;
    case ElementRole::WEBAUTHN:
      result->webauthn_ids.push_back(id);
      break;
    case ElementRole::CURRENT_PASSWORD:
      DCHECK(result->password_id.is_null());
      result->password_id = id;
      break;
    case ElementRole::NEW_PASSWORD:
      DCHECK(result->new_password_id.is_null());
      result->new_password_id = id;
      break;
    case ElementRole::CONFIRMATION_PASSWORD:
      DCHECK(result->confirmation_password_id.is_null());
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
    const autofill::FieldRendererId renderer_id = GetUniqueId();
    field.unique_renderer_id = renderer_id;
    field.id_attribute = StampUniqueSuffix(u"html_id");
    if (field_description.name == kNonimportantValue) {
      field.name = StampUniqueSuffix(u"html_name");
    } else {
      field.name = std::u16string(field_description.name);
    }
    field.name_attribute = field.name;
    field.form_control_type = field_description.form_control_type;
    field.is_focusable = field_description.is_focusable;
    field.is_enabled = field_description.is_enabled;
    field.is_readonly = field_description.is_readonly;
    field.properties_mask = field_description.properties_mask;
    if (field_description.value == kNonimportantValue) {
      field.value = StampUniqueSuffix(u"value");
    } else {
      field.value = field_description.value;
    }
    if (field_description.autocomplete_attribute)
      field.autocomplete_attribute = field_description.autocomplete_attribute;
    if (!field_description.user_input.empty())
      field.user_input = field_description.user_input;
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
  for (autofill::FieldRendererId& id : form_data.username_predictions) {
    if (id.is_null())
      id = GetUniqueId();
  }
  return form_data;
}

// Check that |fields| has a field with unique renderer ID |renderer_id| which
// has the name |element_name| and value |*element_value|. If
// |renderer_id|.is_null(), then instead check that |element_name| and
// |*element_value| are empty. Set |element_kind| to identify the type of the
// field in logging: 'username', 'password', etc. The argument |element_value|
// can be null, in which case all checks involving it are skipped (useful for
// the confirmation password value, which is not represented in PasswordForm).
void CheckField(const std::vector<FormFieldData>& fields,
                autofill::FieldRendererId renderer_id,
                const std::u16string& element_name,
                const std::u16string* element_value,
                const char* element_kind) {
  SCOPED_TRACE(testing::Message("Looking for element of kind ")
               << element_kind);

  if (renderer_id.is_null()) {
    EXPECT_EQ(std::u16string(), element_name);
    if (element_value)
      EXPECT_EQ(std::u16string(), *element_value);
    return;
  }

  auto field_it = base::ranges::find(fields, renderer_id,
                                     &FormFieldData::unique_renderer_id);
  ASSERT_TRUE(field_it != fields.end())
      << "Could not find a field with renderer ID " << renderer_id;

  EXPECT_EQ(element_name, field_it->name);

  std::u16string expected_value =
      field_it->user_input.empty() ? field_it->value : field_it->user_input;

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
           << ", unique id=" << field.unique_renderer_id.value() << "\n";
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
void CheckAllValuesUnique(const ValueElementVector& v) {
  std::set<std::u16string> all_values;
  for (const auto& pair : v) {
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
        EXPECT_FALSE(parsed_form->blocked_by_user);
        EXPECT_EQ(PasswordForm::Type::kFormSubmission, parsed_form->type);
        EXPECT_EQ(test_case.server_side_classification_successful,
                  parsed_form->server_side_classification_successful);
        EXPECT_EQ(test_case.username_may_use_prefilled_placeholder,
                  parsed_form->username_may_use_prefilled_placeholder);
        EXPECT_EQ(test_case.submission_event, parsed_form->submission_event);
        if (test_case.is_new_password_reliable &&
            mode == FormDataParser::Mode::kFilling) {
          EXPECT_EQ(*test_case.is_new_password_reliable,
                    parsed_form->is_new_password_reliable);
        }
        EXPECT_EQ(test_case.accepts_webauthn_credentials &&
                      mode == FormDataParser::Mode::kFilling,
                  parsed_form->accepts_webauthn_credentials);
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
        const absl::optional<FormDataParser::ReadonlyPasswordFields>*
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
                   .value = u"pw",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw",
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
                   .value = u"pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
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
                   .value = u"pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = "password"},
              },
          .is_new_password_reliable = false,
      },
      {
          .description_for_logging = "3 password fields with different values",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"pw1",
                   .form_control_type = "password"},
                  {.value = u"pw2", .form_control_type = "password"},
                  {.value = u"pw3", .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 3,
      },
      {
          .description_for_logging =
              "4 password fields, only the first 3 are considered",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = "password"},
                  {.value = u"pw3", .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "4 password fields, 4th same value as 3rd "
                                     "and 2nd, only the first 3 "
                                     "are considered",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"pw1",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = "password"},
                  {.value = u"pw2", .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "4 password fields, all same value",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"pw",
                   .form_control_type = "password"},
                  {.value = u"pw", .form_control_type = "password"},
                  {.value = u"pw", .form_control_type = "password"},
                  {.value = u"pw", .form_control_type = "password"},
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
                   .value = u"pw",
                   .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .is_focusable = true,
                   .value = u"pw",
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
                   .value = u"",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = u"",
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
                  {.value = u"", .form_control_type = "text"},
                  {.role_filling = ElementRole::USERNAME,
                   .value = u"",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = u"",
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
                   .value = u"",
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
                   .value = u"",
                   .form_control_type = "text"},
                  {.is_focusable = false,
                   .value = u"",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = u"",
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
                   .value = u"",
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
                   .value = u"",
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
               .value = u"12",
               .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .value = u"strong_pw",
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
                   .value = u"np",
                   .form_control_type = "password"},
                  {.form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"np",
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
                   .value = u"pw",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw",
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
      {
          .description_for_logging = "Autocomplete single username",
          .fields = {{.role = ElementRole::USERNAME,
                      .is_focusable = false,
                      .autocomplete_attribute = "username",
                      .form_control_type = "text"}},
      },
  });
}

// Checks that fields with "one-time-code" autocomplete attribute are
// not parsed as usernames or passwords.
TEST(FormParserTest, SkippingFieldsWithOTPAutocomplete) {
  CheckTestData({
      {
          .description_for_logging =
              "The only password field marked as OTP in autocomplete",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "one-time-code",
                   .form_control_type = "password"},
              },
          .fallback_only = true,
      },
      {
          .description_for_logging = "Non-OTP fields are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.autocomplete_attribute = "one-time-code",
                   .form_control_type = "text"},
                  {.autocomplete_attribute = "one-time-code",
                   .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 2,
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
                   .value = u"newpass",
                   .form_control_type = "password"},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "new-password",
                   .value = u"newpass",
                   .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "current-password",
                   .value = u"oldpass",
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
                       FieldPropertiesFlags::kAutofilledOnPageLoad,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .properties_mask = FieldPropertiesFlags::kUserTyped,
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
                       FieldPropertiesFlags::kAutofilledOnUserTrigger,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .properties_mask = FieldPropertiesFlags::kUserTyped,
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
      password_manager::features::kEnablePasswordGenerationForClearTextFields);
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

TEST(FormParserTest, InferConfirmationPasswordField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInferConfirmationPasswordField);
  CheckTestData({
      {
          .description_for_logging = "Infer confirmation password during "
                                     "saving with server prediction.",
          .fields =
              {
                  {
                      .role = ElementRole::NEW_PASSWORD,
                      .value = u"pw",
                      .form_control_type = "password",
                      .prediction = {.type =
                                         autofill::ACCOUNT_CREATION_PASSWORD},
                  },
                  {
                      .role_saving = ElementRole::CONFIRMATION_PASSWORD,
                      .value = u"pw",
                      .form_control_type = "password",
                  },
              },
      },
      {
          .description_for_logging = "Infer confirmation password during "
                                     "saving with auto-complete attribute.",
          .fields =
              {
                  {
                      .role = ElementRole::NEW_PASSWORD,
                      .autocomplete_attribute = "new-password",
                      .value = u"pw",
                      .form_control_type = "password",
                  },
                  {
                      .role_filling = ElementRole::NONE,
                      .role_saving = ElementRole::CONFIRMATION_PASSWORD,
                      .autocomplete_attribute = "off",
                      .value = u"pw",
                      .form_control_type = "password",
                  },
              },
      },
      {
          .description_for_logging =
              "Don't infer confirmation password during saving with "
              "predictions and different passwords.",
          .fields =
              {
                  {
                      .role_filling = ElementRole::NEW_PASSWORD,
                      .role_saving = ElementRole::CURRENT_PASSWORD,
                      .value = u"pw1",
                      .form_control_type = "password",
                      .prediction = {.type =
                                         autofill::ACCOUNT_CREATION_PASSWORD},
                  },
                  {
                      .role_saving = ElementRole::NEW_PASSWORD,
                      .value = u"pw2",
                      .form_control_type = "password",
                  },
              },
      },
      {
          .description_for_logging =
              "Don't infer confirmation password during saving with "
              "autocomplete attribute and different passwords.",
          .fields =
              {
                  {
                      .role = ElementRole::NEW_PASSWORD,
                      .autocomplete_attribute = "new-password",
                      .value = u"pw1",
                      .form_control_type = "password",
                  },
                  {
                      .role = ElementRole::NONE,
                      .value = u"pw2",
                      .form_control_type = "password",
                  },
              },
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
          .server_side_classification_successful = true,
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
      {
          .description_for_logging = "Username not a placeholder",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME_AND_EMAIL_ADDRESS,
                                  .may_use_prefilled_placeholder = false}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::PASSWORD,
                                  .may_use_prefilled_placeholder = false}},
              },
          .server_side_classification_successful = true,
          .username_may_use_prefilled_placeholder = false,
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
                   .properties_mask = FieldPropertiesFlags::kUserTyped,
                   .form_control_type = "text"},
                  {.is_focusable = true, .form_control_type = "text"},
                  {.is_focusable = false, .form_control_type = "password"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::kAutofilled,
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::kUserTyped,
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
                   .properties_mask = FieldPropertiesFlags::kAutofilled,
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::kAutofilled,
                   .form_control_type = "password"},
                  {.is_focusable = true,
                   .value = u"",
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
  const ValueElementVector kPasswords = {
      {u"a", u"p1"},
      {u"b", u"p3"},
  };
  const ValueElementVector kUsernames = {
      {u"b", u"chosen"},
      {u"a", u"first"},
  };
  CheckTestData({
      {
          .description_for_logging = "It is always the first field name which "
                                     "is associated with a duplicated password "
                                     "value",
          .fields =
              {
                  {.value = u"a",
                   .name = u"p1",
                   .form_control_type = "password"},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = u"b",
                   .name = u"chosen",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"a",
                   .form_control_type = "password"},
                  {.value = u"a",
                   .name = u"first",
                   .form_control_type = "text"},
                  {.value = u"a", .form_control_type = "text"},
                  {.value = u"b",
                   .name = u"p3",
                   .form_control_type = "password"},
                  {.value = u"b", .form_control_type = "password"},
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
                  {.value = u"", .form_control_type = "password"},
                  {.role_filling = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = u"",
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
                   .form_control_type = "password"},
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.value = u"", .form_control_type = "password"},
                  {.value = u"", .form_control_type = "password"},
              },
          .number_of_all_possible_passwords = 0,
      },
      {
          .description_for_logging = "Empty values don't get added to "
                                     "all_possible_passwords even if form gets "
                                     "parsed",
          .fields =
              {
                  {.value = u"", .form_control_type = "password"},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = "password"},
                  {.form_control_type = "text"},
                  {.form_control_type = "text"},
                  {.value = u"", .form_control_type = "password"},
                  {.value = u"", .form_control_type = "password"},
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
                   .value = u"",
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
                                     "CVC pattern, ignore that one.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.name = u"verification_type",
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
                   .name = u"verification_type",
                   .form_control_type = "password"},
              },
          .fallback_only = true,
      },
  });
}

// The parser should avoid identifying Credit Card Number fields as passwords
// if the server identifies the fields as CC Number fields. This should be
// relatively safe as it should be unlikely that the server misclassifies a
// field as a CC Number field.
TEST(FormParserTest, CCNumber) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: CREDIT_CARD_NUMBER.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password",
                   .prediction = {.type = autofill::CREDIT_CARD_NUMBER}},
              },
          .fallback_only = true,
      },
      {
          .description_for_logging =
              "Name of 'ccnumber' matches the CC Number regex pattern (but "
              "there is no confirmation from the server), ignore that one.",
          .fields =
              {
                  {.role = ElementRole::USERNAME, .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .name = u"ccnumber",
                   .form_control_type = "password"},
              },
          .fallback_only = false,
      },
      // The following describes the status quo for documentation purposes. It
      // is probably not desirable. If we have high confidence in all credit
      // card fields, the password manager should probably ignore those fields
      // entirely.
      {
          .description_for_logging = "Example where CC Number and Expiration "
                                     "date are both password fields.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .name = u"cardholder",
                   .form_control_type = "text",
                   .prediction = {.type = autofill::CREDIT_CARD_NAME_FULL}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .name = u"ccnumber",
                   .form_control_type = "password",
                   .prediction = {.type = autofill::CREDIT_CARD_NUMBER}},
                  {.name = u"expiration",
                   .form_control_type = "text",
                   .prediction =
                       {.type = autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}},
                  {.role = ElementRole::NEW_PASSWORD,
                   .name = u"cvc",
                   .form_control_type = "password",
                   .prediction = {.type =
                                      autofill::CREDIT_CARD_VERIFICATION_CODE}},
              },
          .fallback_only = true,
      },
  });
}

// TODO(crbug.com/1382805): Remove this test once the new regex launched.
// The parser should avoid identifying Social Security number and
// one time password fields as passwords.
TEST(FormParserTest, SSN_and_OTP_Old_Regex) {
  for (const char16_t* field_name :
       {u"SocialSecurityNumber", u"OneTimePassword", u"SMS-token"}) {
    CheckTestData({
        {
            .description_for_logging = "Field name matches the SSN/OTP pattern,"
                                       "Ignore that one.",
            .fields =
                {
                    {.role = ElementRole::USERNAME,
                     .form_control_type = "text"},
                    {.name = field_name, .form_control_type = "password"},
                    {.role = ElementRole::CURRENT_PASSWORD,
                     .form_control_type = "password"},
                },
            // The result should be trusted for more than just fallback, because
            // there is an actual password field present.
            .fallback_only = false,
        },
        {
            .description_for_logging = "Create a fallback for the only password"
                                       "field being an SSN/OTP field",
            .fields =
                {
                    {.role = ElementRole::USERNAME,
                     .form_control_type = "text"},
                    {.role = ElementRole::CURRENT_PASSWORD,
                     .name = field_name,
                     .form_control_type = "password"},
                },
            .fallback_only = true,
        },
    });
  }
}

TEST(FormParserTest, SSN_and_OTP) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kNewRegexForOtpFields);
  for (const char16_t* field_name :
       {u"SocialSecurityNumber", u"OneTimePassword", u"SMS-token", u"otp-code",
        u"input_SMS", u"second.factor", u"2FA", u"Sms{Otp}",
        u"login$$verif_vcode"}) {
    CheckTestData({
        {
            .description_for_logging = "Field name matches the SSN/OTP pattern,"
                                       "Ignore that one.",
            .fields =
                {
                    {.role = ElementRole::USERNAME,
                     .form_control_type = "text"},
                    {.name = field_name, .form_control_type = "password"},
                    {.role = ElementRole::CURRENT_PASSWORD,
                     .form_control_type = "password"},
                },
            // The result should be trusted for more than just fallback, because
            // there is an actual password field present.
            .fallback_only = false,
        },
        {
            .description_for_logging = "Create a fallback for the only password"
                                       "field being an SSN/OTP field",
            .fields =
                {
                    {.role = ElementRole::USERNAME,
                     .form_control_type = "text"},
                    {.role = ElementRole::CURRENT_PASSWORD,
                     .name = field_name,
                     .form_control_type = "password"},
                },
            .fallback_only = true,
        },
    });
  }
}

TEST(FormParserTest, OtpRegexMetric) {
  base::HistogramTester histogram_tester;
  CheckTestData({{
      .fields =
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.name = u"OneTimePassword", .form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      .fallback_only = false,
  }});
  // Two samples because |CheckTestData| parses the form in two modes: filling
  // and saving.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ParserDetectedOtpFieldWithRegex", true, 2);
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
                       FieldPropertiesFlags::kAutofilledOnPageLoad,
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
                   .value = u"",
                   .form_control_type = "text",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .value = u"",
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
                   .value = u"",
                   .form_control_type = "text"},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = "text"},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
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
                   .value = u"",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = "password"},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .value = u"",
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
                       FieldPropertiesFlags::kAutofilledOnPageLoad,
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
  CheckTestData({
      {
          .description_for_logging = "Form with changed by JavaScript values",
          // Tests that typed values are taken as username, password and
          // new password instead of values that are set by JavaScript.
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = u"js_username",
                   .user_input = u"typed_username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"js_password",
                   .user_input = u"typed_password",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"js_new_password",
                   .user_input = u"typed_new_password",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "Form with cleared by JavaScript values",
          // Tests that typed values are taken as username, password and
          // new password instead of values that are cleared by JavaScript.
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = u"",
                   .user_input = u"typed_username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
                   .user_input = u"typed_password",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"",
                   .user_input = u"typed_new_password",
                   .form_control_type = "password"},
              },
      },
      {
          .description_for_logging = "Form autocomplete with cleared by JavaScript values",
          // Username autocomplete tests that typed values are taken as username, password and
          // new password instead of values that are cleared by JavaScript.
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .value = u"",
                   .user_input = u"typed_username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
                   .user_input = u"typed_password",
                   .form_control_type = "password"},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"",
                   .user_input = u"typed_new_password",
                   .form_control_type = "password"},
              },
      },
  });
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

// Invalid form URLs should cause the parser to fail.
TEST(FormParserTest, InvalidURL) {
  FormParsingTestCase form_desc = {
      .fields =
          {
              {.form_control_type = "text"},
              {.form_control_type = "password"},
          },
  };
  FormPredictions no_predictions;
  ParseResultIds dummy;
  FormData form_data =
      GetFormDataAndExpectation(form_desc, &no_predictions, &dummy, &dummy);
  // URL comes from https://crbug.com/1075515.
  form_data.url = GURL("FilEsysteM:htTp:E=/.");
  FormDataParser parser;
  EXPECT_FALSE(parser.Parse(form_data, FormDataParser::Mode::kFilling));
  EXPECT_FALSE(parser.Parse(form_data, FormDataParser::Mode::kSaving));
}

TEST(FormParserTest, FindUsernameInPredictions_SkipPrediction) {
  // Searching username field should skip prediction that is less
  // likely to be user interaction. For example, if a field has no
  // user input while others have, the field cannot be an username
  // field.

  // Create a form containing username, email, id, password, submit.
  const FormParsingTestCase form_desc = {
      .fields = {
          {.name = u"username", .form_control_type = "text"},
          {.name = u"email", .form_control_type = "text"},
          {.name = u"id", .form_control_type = "text"},
          {.name = u"password", .form_control_type = "password"},
          {.name = u"submit", .form_control_type = "submit"},
      }};

  FormPredictions no_predictions;
  ParseResultIds dummy;
  const FormData form_data =
      GetFormDataAndExpectation(form_desc, &no_predictions, &dummy, &dummy);

  // Add all form fields in ProcessedField. A user typed only into
  // "id" and "password" fields. So, the prediction for "email" field
  // should be ignored despite it is more reliable than prediction for
  // "id" field.
  std::vector<ProcessedField> processed_fields;
  for (const auto& form_field_data : form_data.fields)
    processed_fields.push_back(ProcessedField{.field = &form_field_data});

  processed_fields[2].interactability = Interactability::kCertain;  // id
  processed_fields[3].interactability = Interactability::kCertain;  // password

  // Add predictions for "email" and "id" fields. The "email" is in
  // front of "id", indicating "email" is more reliable.
  const std::vector<autofill::FieldRendererId> predictions = {
      form_data.fields[1].unique_renderer_id,  // email
      form_data.fields[2].unique_renderer_id,  // id
  };

  // Now search the username field. The username field is supposed to
  // be "id", not "email".
  const autofill::FormFieldData* field_data = FindUsernameInPredictions(
      predictions, processed_fields, Interactability::kCertain);
  ASSERT_TRUE(field_data);
  EXPECT_EQ(u"id", field_data->name);
}

// Tests that the form parser is not considering fields with values consisting
// of one repeated non alphanumeric symbol for saving.
TEST(FormParserTest, SkipHiddenValueField) {
  std::vector<FormParsingTestCase> test_cases = {
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.value = u"***********",
                   .name = u"password",
                   .form_control_type = "password"},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.value = u"**",
                   .name = u"password",
                   .form_control_type = "password"},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.value = u"••••••••",
                   .name = u"password",
                   .form_control_type = "password"},
              },
      }};
  for (const auto& form_desc : test_cases) {
    FormPredictions no_predictions;
    ParseResultIds dummy;
    FormData form_data =
        GetFormDataAndExpectation(form_desc, &no_predictions, &dummy, &dummy);
    FormDataParser parser;
    EXPECT_TRUE(parser.Parse(form_data, FormDataParser::Mode::kFilling));
    EXPECT_FALSE(parser.Parse(form_data, FormDataParser::Mode::kSaving));
  }
}

// Tests that the form parser is not considering fields with values consisting
// of one repeated non alphanumeric symbol for saving.
TEST(FormParserTest, DontSkipNotHiddenValues) {
  std::vector<FormParsingTestCase> test_cases = {
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.value = u"a*******a",
                   .name = u"password",
                   .form_control_type = "password"},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.value = u".....*****",
                   .name = u"password",
                   .form_control_type = "password"},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.value = u"0 0 0 0 0",
                   .name = u"password",
                   .form_control_type = "password"},
              },
      }};

  for (const auto& form_desc : test_cases) {
    FormPredictions no_predictions;
    ParseResultIds dummy;
    FormData form_data =
        GetFormDataAndExpectation(form_desc, &no_predictions, &dummy, &dummy);
    FormDataParser parser;
    EXPECT_TRUE(parser.Parse(form_data, FormDataParser::Mode::kFilling));
    EXPECT_TRUE(parser.Parse(form_data, FormDataParser::Mode::kSaving));
  }
}

// Tests that 'new-password' autocomplete attribute is ignored when two
// or more fields that have it have different values.
TEST(FormParserTest, AutocompleteAttributesError) {
  CheckTestData(
      {{
           .description_for_logging =
               "Wrong autocomplete attributes, 2 fields.",
           .fields =
               {
                   {.role_filling = ElementRole::NEW_PASSWORD,
                    .role_saving = ElementRole::CURRENT_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"oldpass",
                    .name = u"password1",
                    .form_control_type = "password"},
                   {.role_filling = ElementRole::CONFIRMATION_PASSWORD,
                    .role_saving = ElementRole::NEW_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"newpass",
                    .name = u"password2",
                    .form_control_type = "password"},
               },
       },
       {
           .description_for_logging =
               "Wrong autocomplete attributes, 3 fields.",
           .fields =
               {
                   {.role_filling = ElementRole::NEW_PASSWORD,
                    .role_saving = ElementRole::CURRENT_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"oldpass",
                    .name = u"password1",
                    .form_control_type = "password"},
                   {.role_filling = ElementRole::CONFIRMATION_PASSWORD,
                    .role_saving = ElementRole::NEW_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"newpass",
                    .name = u"password2",
                    .form_control_type = "password"},
                   {.role_saving = ElementRole::CONFIRMATION_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"newpass",
                    .name = u"password3",
                    .form_control_type = "password"},
               },
       }});
}

// Tests that if the field is parsed as username based on server predictions,
// than it cannot be picked as password based on local heuristics.
TEST(FormParserTest, UsernameWithTypePasswordAndServerPredictions) {
  CheckTestData({
      {
          .description_for_logging =
              "Username with server predictions and type 'password'",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .value = u"testusername",
                   .name = u"field1",
                   .form_control_type = "password",
                   .prediction = {.type = autofill::USERNAME}},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"testpass",
                   .name = u"field2",
                   .form_control_type = "password"},
              },
      },
  });
}

// Tests that if a field is marked as autofill="webauthn" then the
// `accepts_webauthn_credentials` flag is set.
TEST(FormParserTest, AcceptsWebAuthnCredentials) {
  CheckTestData({
      {
          .description_for_logging = "Field tagged with autofill=\"webauthn\"",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "webauthn",
                   .value = u"rosalina",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"luma",
                   .name = u"password",
                   .form_control_type = "password"},
              },
          .accepts_webauthn_credentials = true,
      },
  });
}

// Tests that if there is a single field marked as autofill="webauthn" then the
// form is parsed and the `accepts_webauthn_credentials` flag is set.
// Regression test for crbug.com/1366006.
TEST(FormParserTest, SingleFieldAcceptsWebAuthnCredentials) {
  CheckTestData({
      {
          .description_for_logging =
              "Single field tagged with autofill=\"webauthn\"",
          .fields =
              {
                  {.role_filling = ElementRole::WEBAUTHN,
                   .autocomplete_attribute = "webauthn",
                   .value = u"rosalina",
                   .name = u"username",
                   .form_control_type = "text"},
              },
          .accepts_webauthn_credentials = true,
      },
  });
}

// Tests that if a field is marked as autofill="username webauthn" then the
// `accepts_webauthn_credentials` flag is set.
TEST(FormParserTest, AcceptsUsernameWebAuthnCredentials) {
  CheckTestData({
      {
          .description_for_logging =
              "Field tagged with autofill=\"username webauthn\"",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username webauthn",
                   .value = u"rosalina",
                   .name = u"username",
                   .form_control_type = "text"},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"luma",
                   .name = u"password",
                   .form_control_type = "password"},
              },
          .accepts_webauthn_credentials = true,
      },
  });
}

}  // namespace

}  // namespace password_manager
