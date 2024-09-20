// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FieldPropertiesFlags;
using autofill::FormControlType;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::mojom::SubmissionIndicatorEvent;

namespace password_manager {

namespace {

using testing::IsNull;
using testing::NotNull;

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
  // Text fields with new password server prediction.
  TYPE_TEXT_NEW_PASSWORD_FIELD,
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
  const std::u16string_view id_attribute = kNonimportantValue;
  const std::u16string_view name = kNonimportantValue;
  FormControlType form_control_type = FormControlType::kInputText;
  autofill::FieldType predicted_type = autofill::MAX_VALID_FIELD_TYPE;
  bool may_use_prefilled_placeholder = false;
  // If not -1, indicates on which rank among predicted usernames this should
  // be. Unused ranks will be padded with unique IDs (not found in any fields).
  int predicted_username = -1;
  uint64_t max_length_attr = FormFieldData::kDefaultMaxLength;
};

// Describes a test case for the parser.
struct FormParsingTestCase {
  const char* description_for_logging;
  std::vector<FieldDataDescription> fields;
  // -1 just mean no checking.
  int number_of_all_alternative_passwords = -1;
  int number_of_all_alternative_usernames = -1;
  // null means no checking
  raw_ptr<const AlternativeElementVector> all_alternative_passwords = nullptr;
  raw_ptr<const AlternativeElementVector> all_alternative_usernames = nullptr;
  bool server_side_classification_successful = true;
  bool username_may_use_prefilled_placeholder = false;
  std::optional<FormDataParser::ReadonlyPasswordFields> readonly_status;
  std::optional<FormDataParser::ReadonlyPasswordFields>
      readonly_status_for_saving;
  std::optional<FormDataParser::ReadonlyPasswordFields>
      readonly_status_for_filling;
  // If the result should be marked as only useful for fallbacks.
  bool fallback_only = false;
  SubmissionIndicatorEvent submission_event = SubmissionIndicatorEvent::NONE;
  std::optional<bool> is_new_password_reliable;
  bool form_has_autofilled_value = false;
  bool accepts_webauthn_credentials = false;
};

// Describes which renderer IDs are expected for username/password fields
// identified in a PasswordForm.
struct ParseResultIds {
  autofill::FieldRendererId username_id;
  autofill::FieldRendererId password_id;
  autofill::FieldRendererId new_password_id;
  autofill::FieldRendererId confirmation_password_id;
  std::vector<autofill::FieldRendererId> webauthn_ids;
  autofill::FieldRendererId manual_generation_enabled_id;

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
    case ElementRole::TYPE_TEXT_NEW_PASSWORD_FIELD:
      result->manual_generation_enabled_id = id;
      break;
  }
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
    if (element_value) {
      EXPECT_EQ(std::u16string(), *element_value);
    }
    return;
  }

  auto field_it =
      base::ranges::find(fields, renderer_id, &FormFieldData::renderer_id);
  ASSERT_TRUE(field_it != fields.end())
      << "Could not find a field with renderer ID " << renderer_id;

  EXPECT_EQ(element_name, field_it->name());

  std::u16string expected_value = field_it->user_input().empty()
                                      ? field_it->value()
                                      : field_it->user_input();

  if (element_value) {
    EXPECT_EQ(expected_value, *element_value);
  }
}

// Describes the |form_data| including field values and names. Use this in
// SCOPED_TRACE if other logging messages might refer to the form.
testing::Message DescribeFormData(const FormData& form_data) {
  testing::Message result;
  result << "Form contains " << form_data.fields().size() << " fields:\n";
  for (const FormFieldData& field : form_data.fields()) {
    result << "type="
           << autofill::FormControlTypeToString(field.form_control_type())
           << ", name=" << field.name() << ", value=" << field.value()
           << ", unique id=" << field.renderer_id().value() << "\n";
  }
  return result;
}

// Check that the information distilled from |form_data| into |password_form| is
// matching |expectations|.
void CheckPasswordFormFields(const FormParsingResult& parsing_result,
                             const FormData& form_data,
                             const ParseResultIds& expectations) {
  SCOPED_TRACE(DescribeFormData(form_data));
  CheckField(form_data.fields(), expectations.username_id,
             parsing_result.password_form->username_element,
             &parsing_result.password_form->username_value, "username");
  EXPECT_EQ(expectations.username_id,
            parsing_result.password_form->username_element_renderer_id);

  CheckField(form_data.fields(), expectations.password_id,
             parsing_result.password_form->password_element,
             &parsing_result.password_form->password_value, "password");
  EXPECT_EQ(expectations.password_id,
            parsing_result.password_form->password_element_renderer_id);

  CheckField(form_data.fields(), expectations.new_password_id,
             parsing_result.password_form->new_password_element,
             &parsing_result.password_form->new_password_value, "new_password");

  CheckField(form_data.fields(), expectations.confirmation_password_id,
             parsing_result.password_form->confirmation_password_element,
             nullptr, "confirmation_password");

  EXPECT_EQ(expectations.manual_generation_enabled_id,
            parsing_result.manual_generation_enabled_field);
}

// Checks that in a vector of pairs of string16s, all the first parts of the
// pairs (which represent element values) are unique.
void CheckAllValuesUnique(const AlternativeElementVector& v) {
  std::set<std::u16string> all_values;
  for (const auto& element : v) {
    auto insertion = all_values.insert(element.value);
    EXPECT_TRUE(insertion.second) << element.value << " is duplicated";
  }
}

// Creates a simple field with `type` and `value`. Requires an
// `AutofillTestEnvironment` instance to generate the renderer id.
FormFieldData CreateField(FormControlType type, std::u16string value) {
  FormFieldData field;
  field.set_form_control_type(type);
  field.set_value(std::move(value));
  field.set_renderer_id(autofill::test::MakeFieldRendererId());
  return field;
}

class FormParserTest : public testing::Test {
 protected:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;

  std::u16string GetFieldNameByIndex(size_t index) {
    return u"field" + base::NumberToString16(index);
  }

  // Returns numbers which are distinct from each other within the scope of one
  // test.
  autofill::FieldRendererId GetUniqueId() {
    return autofill::FieldRendererId(renderer_id_counter_++);
  }

  // Use to add a numeric suffix which is unique in the scope of the test.
  std::u16string StampUniqueSuffix(const char16_t* base_str) {
    return base_str + std::u16string(u"_") +
           base::NumberToString16(GetUniqueId().value());
  }

  // Creates a FormData to be fed to the parser. Includes FormFieldData as
  // described in |fields_description|. Generates |fill_result| and
  // |save_result| expectations about the result in FILLING and SAVING mode,
  // respectively. Also fills |predictions| with the predictions contained in
  // FieldDataDescriptions.
  FormData GetFormDataAndExpectation(const FormParsingTestCase& test_case,
                                     FormPredictions* predictions,
                                     ParseResultIds* fill_result,
                                     ParseResultIds* save_result) {
    FormData form_data;
    form_data.set_action(GURL("http://example1.com"));
    form_data.set_url(GURL("http://example2.com"));
    form_data.set_submission_event(test_case.submission_event);
    std::vector<autofill::FieldRendererId> username_predictions;
    for (const FieldDataDescription& field_description : test_case.fields) {
      FormFieldData field;
      const autofill::FieldRendererId renderer_id = GetUniqueId();
      field.set_renderer_id(renderer_id);
      if (field_description.id_attribute == kNonimportantValue) {
        field.set_id_attribute(StampUniqueSuffix(u"html_id"));
      } else {
        field.set_id_attribute(std::u16string(field_description.id_attribute));
      }
      if (field_description.name == kNonimportantValue) {
        field.set_name(StampUniqueSuffix(u"html_name"));
      } else {
        field.set_name(std::u16string(field_description.name));
      }
      field.set_name_attribute(field.name());
      field.set_form_control_type(field_description.form_control_type);
      field.set_is_focusable(field_description.is_focusable);
      field.set_is_enabled(field_description.is_enabled);
      field.set_is_readonly(field_description.is_readonly);
      field.set_properties_mask(field_description.properties_mask);
      field.set_max_length(field_description.max_length_attr);
      if (field_description.value == kNonimportantValue) {
        field.set_value(StampUniqueSuffix(u"value"));
      } else {
        field.set_value(field_description.value);
      }
      if (field_description.autocomplete_attribute) {
        field.set_autocomplete_attribute(
            field_description.autocomplete_attribute);
      }
      if (!field_description.user_input.empty()) {
        field.set_user_input(field_description.user_input);
      }
      test_api(form_data).Append(field);
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
      if (field_description.predicted_type != autofill::MAX_VALID_FIELD_TYPE) {
        predictions->fields.emplace_back(
            renderer_id, autofill::FieldSignature(123),
            field_description.predicted_type,
            /*may_use_prefilled_placeholder=*/
            field_description.may_use_prefilled_placeholder,
            /*is_override=*/false);
      }
      if (field_description.predicted_username >= 0) {
        size_t index =
            static_cast<size_t>(field_description.predicted_username);
        if (username_predictions.size() <= index) {
          username_predictions.resize(index + 1);
        }
        username_predictions[index] = field.renderer_id();
      }
    }
    // Fill unused ranks in predictions with fresh IDs to check that those are
    // correctly ignored. In real situation, this might correspond, e.g., to
    // fields which were not fillable and hence dropped from the selection.
    for (autofill::FieldRendererId& id : username_predictions) {
      if (id.is_null()) {
        id = GetUniqueId();
      }
    }
    form_data.set_username_predictions(std::move(username_predictions));
    return form_data;
  }

  // Iterates over |test_cases|, creates a FormData for each, runs the parser
  // and checks the results.
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

        FormParsingResult parsing_result = parser.ParseAndReturnParsingResult(
            form_data, mode, /*stored_usernames=*/{});
        const ParseResultIds& expected_ids =
            mode == FormDataParser::Mode::kFilling ? fill_result : save_result;

        if (expected_ids.IsEmpty()) {
          EXPECT_THAT(parsing_result.password_form, IsNull())
              << "Expected no parsed results";
        } else {
          ASSERT_THAT(parsing_result.password_form, NotNull())
              << "Expected successful parsing";
          EXPECT_EQ(PasswordForm::Scheme::kHtml,
                    parsing_result.password_form->scheme);
          EXPECT_FALSE(parsing_result.password_form->blocked_by_user);
          EXPECT_EQ(PasswordForm::Type::kFormSubmission,
                    parsing_result.password_form->type);
          EXPECT_EQ(test_case.server_side_classification_successful,
                    parsing_result.password_form
                        ->server_side_classification_successful);
          EXPECT_EQ(test_case.username_may_use_prefilled_placeholder,
                    parsing_result.password_form
                        ->username_may_use_prefilled_placeholder);
          EXPECT_EQ(test_case.submission_event,
                    parsing_result.password_form->submission_event);
          if (test_case.is_new_password_reliable &&
              mode == FormDataParser::Mode::kFilling) {
            EXPECT_EQ(*test_case.is_new_password_reliable,
                      parsing_result.is_new_password_reliable);
          }
          EXPECT_EQ(test_case.accepts_webauthn_credentials &&
                        mode == FormDataParser::Mode::kFilling,
                    parsing_result.password_form->accepts_webauthn_credentials);
          EXPECT_EQ(test_case.form_has_autofilled_value,
                    parsing_result.password_form->form_has_autofilled_value);

          CheckPasswordFormFields(parsing_result, form_data, expected_ids);
          CheckAllValuesUnique(
              parsing_result.password_form->all_alternative_passwords);
          CheckAllValuesUnique(
              parsing_result.password_form->all_alternative_usernames);
          if (test_case.number_of_all_alternative_passwords >= 0) {
            EXPECT_EQ(
                static_cast<size_t>(
                    test_case.number_of_all_alternative_passwords),
                parsing_result.password_form->all_alternative_passwords.size());
          }
          if (test_case.all_alternative_passwords) {
            EXPECT_EQ(*test_case.all_alternative_passwords,
                      parsing_result.password_form->all_alternative_passwords);
          }
          if (test_case.number_of_all_alternative_usernames >= 0) {
            EXPECT_EQ(
                static_cast<size_t>(
                    test_case.number_of_all_alternative_usernames),
                parsing_result.password_form->all_alternative_usernames.size());
          }
          if (test_case.all_alternative_usernames) {
            EXPECT_EQ(*test_case.all_alternative_usernames,
                      parsing_result.password_form->all_alternative_usernames);
          }
          if (mode == FormDataParser::Mode::kSaving) {
            EXPECT_EQ(test_case.fallback_only,
                      parsing_result.password_form->only_for_fallback);
          }
        }
        if (test_case.readonly_status) {
          EXPECT_EQ(*test_case.readonly_status, parser.readonly_status());
        } else {
          const std::optional<FormDataParser::ReadonlyPasswordFields>*
              expected_readonly_status =
                  mode == FormDataParser::Mode::kSaving
                      ? &test_case.readonly_status_for_saving
                      : &test_case.readonly_status_for_filling;
          if (expected_readonly_status->has_value()) {
            EXPECT_EQ(*expected_readonly_status, parser.readonly_status());
          }
        }
      }
    }
  }

  uint32_t renderer_id_counter_ = 10;
};

TEST_F(FormParserTest, NotPasswordForm) {
  CheckTestData({
      {
          .description_for_logging = "No fields",
          .fields = {},
      },
      {
          .description_for_logging = "No password fields",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
              },
          .number_of_all_alternative_passwords = 0,
          .number_of_all_alternative_usernames = 0,
      },
  });
}

TEST_F(FormParserTest, SkipNotTextFields) {
  CheckTestData({
      {
          .description_for_logging =
              "A 'select' between username and password fields",
          .fields =
              {
                  {.role = ElementRole::USERNAME},
                  {.form_control_type = FormControlType::kSelectOne},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 1,
          .number_of_all_alternative_usernames = 1,
      },
  });
}

TEST_F(FormParserTest, OnlyPasswordFields) {
  CheckTestData({
      {
          .description_for_logging = "1 password field",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 1,
          .number_of_all_alternative_usernames = 0,
      },
      {
          .description_for_logging =
              "2 password fields, new and confirmation password",
          .fields =
              {
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .is_new_password_reliable = false,
      },
      {
          .description_for_logging = "3 password fields with different values",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"pw1",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"pw3",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 3,
      },
      {
          .description_for_logging =
              "4 password fields, only the first 3 are considered",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"pw1",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"pw3",
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"pw2",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging = "4 password fields, all same value",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
  });
}

TEST_F(FormParserTest, TestFocusability) {
  CheckTestData({
      {
          .description_for_logging =
              "non-focusable fields are considered when there are no focusable "
              "fields",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "non-focusable should be skipped when there are focusable fields",
          .fields =
              {
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "non-focusable text fields before password",
          .fields =
              {
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::USERNAME,
                   .is_focusable = false,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_usernames = 2,
      },
      {
          .description_for_logging =
              "focusable and non-focusable text fields before password",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging = "many passwords, some of them focusable",
          .fields =
              {
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = true,
                   .value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .is_focusable = true,
                   .value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
              },
          // 9 distinct values in 10 password fields:
          .number_of_all_alternative_passwords = 9,
      },
  });
}

TEST_F(FormParserTest, TextAndPasswordFields) {
  CheckTestData({
      {
          .description_for_logging = "Simple empty sign-in form",
          // Forms with empty fields cannot be saved, so the parsing result for
          // saving is empty.
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword},
              },
          // all_alternative_* only count fields with non-empty values.
          .number_of_all_alternative_passwords = 0,
          .number_of_all_alternative_usernames = 0,
      },
      {
          .description_for_logging = "Simple sign-in form with filled data",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 1,
      },
      {
          .description_for_logging =
              "Empty sign-in form with an extra text field",
          .fields =
              {
                  {.value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::USERNAME,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Non-empty sign-in form with an extra text field",
          .fields =
              {
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::USERNAME,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Empty sign-in form with an extra invisible text field",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = false,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Non-empty sign-in form with an extra invisible text field",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Simple empty sign-in form with empty password",
          // Empty password, nothing to save.
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
  });
}

TEST_F(FormParserTest, TextFieldValueIsNotUsername) {
  CheckTestData({{
      .description_for_logging = "Text field value is unlikely username so it "
                                 "should be ignored on saving",
      .fields =
          {
              {.role_filling = ElementRole::USERNAME,
               .value = u"12",
               .form_control_type = FormControlType::kInputText},
              {.role = ElementRole::CURRENT_PASSWORD,
               .value = u"strong_pw",
               .form_control_type = FormControlType::kInputPassword},
          },
  }});
}

TEST_F(FormParserTest, TestAutocomplete) {
  CheckTestData({
      {
          .description_for_logging =
              "All alternative password autocomplete attributes and some "
              "fields without autocomplete",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"np",
                   .form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"np",
                   .form_control_type = FormControlType::kInputPassword},
              },
          // 4 distinct password values in 5 password fields
          .number_of_all_alternative_passwords = 4,
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "Non-password autocomplete attributes are skipped",
          .fields =
              {
                  {.autocomplete_attribute = "email",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .value = u"pw",
                   .form_control_type = FormControlType::kInputPassword},
                  // NB: 'password' is not a valid autocomplete type hint.
                  {.autocomplete_attribute = "password",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 3,
          .number_of_all_alternative_usernames = 2,
      },
      {
          .description_for_logging =
              "Basic heuristics kick in if autocomplete analysis fails",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "email",
                   .form_control_type = FormControlType::kInputText},
                  // NB: 'password' is not a valid autocomplete type hint.
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Multiple username autocomplete attributes, fallback to base "
              "heuristics",
          .fields =
              {
                  {.autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
                  // Valid information about form sections, in addition to the
                  // username hint.
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "new-password current-password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging = "Ignored autocomplete attributes",
          .fields =
              {
                  // 'off' is ignored.
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "off",
                   .form_control_type = FormControlType::kInputText},
                  // Invalid composition, the parser ignores all but the last
                  // token.
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "new-password abc",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = true,
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = true,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = false,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging = "Autocomplete single username",
          .fields = {{.role = ElementRole::USERNAME,
                      .is_focusable = false,
                      .autocomplete_attribute = "username",
                      .form_control_type = FormControlType::kInputText}},
      },
  });
}

// Checks that fields with "one-time-code" autocomplete attribute are
// not parsed as usernames or passwords.
TEST_F(FormParserTest, SkippingFieldsWithOTPAutocomplete) {
  CheckTestData({
      {
          .description_for_logging =
              "The only password field marked as OTP in autocomplete",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "one-time-code",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = true,
      },
      {
          .description_for_logging = "Non-OTP fields are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "one-time-code",
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "one-time-code",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 2,
      },
  });
}

TEST_F(FormParserTest, DisabledFields) {
  CheckTestData({
      {
          .description_for_logging = "The disabled attribute is ignored",
          .fields =
              {
                  {.is_enabled = true,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::USERNAME,
                   .is_enabled = false,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_enabled = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_enabled = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 2,
      },
  });
}

TEST_F(FormParserTest, SkippingFieldsWithCreditCardFields) {
  base::test::ScopedFeatureList feature_list;
  CheckTestData({
      {
          .description_for_logging =
              "Simple form, all fields are credit-card-related",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "cc-name",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "cc-any-string",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = true,
      },
      {
          .description_for_logging = "Non-CC fields are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "cc-name",
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "cc-any-string",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 2,
      },
  });
}

TEST_F(FormParserTest, ReadonlyFields) {
  CheckTestData({
      {
          .description_for_logging = "For usernames, readonly does not matter",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_readonly = true,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "For passwords, readonly means: 'give up', perhaps there is a "
              "virtual keyboard, filling might be ignored",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "new-password",
                   .value = u"newpass",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "new-password",
                   .value = u"newpass",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "current-password",
                   .value = u"oldpass",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging = "And passwords already filled by user or "
                                     "Chrome on pageload are accepted even if "
                                     "readonly",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .properties_mask =
                       FieldPropertiesFlags::kAutofilledOnPageLoad,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .properties_mask = FieldPropertiesFlags::kUserTyped,
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_readonly = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 3,
          .form_has_autofilled_value = true,
      },
      {
          .description_for_logging = "And passwords already filled by user or "
                                     "Chrome with FOAS are accepted even if "
                                     "readonly",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .properties_mask =
                       FieldPropertiesFlags::kAutofilledOnUserTrigger,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = true,
                   .properties_mask = FieldPropertiesFlags::kUserTyped,
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_readonly = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 3,
          .form_has_autofilled_value = true,
      },
  });
}

TEST_F(FormParserTest, ServerPredictionsForClearTextPasswordFields) {
  CheckTestData({
      {
          .description_for_logging = "Server prediction for account change "
                                     "password and username field.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME_AND_EMAIL_ADDRESS},
                  {.role = ElementRole::TYPE_TEXT_NEW_PASSWORD_FIELD,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::NEW_PASSWORD},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account change password field only.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::TYPE_TEXT_NEW_PASSWORD_FIELD,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::NEW_PASSWORD},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account password and username field.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME_AND_EMAIL_ADDRESS},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::PASSWORD},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account password field only.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::PASSWORD},
              },
      },
      {
          .description_for_logging = "Server prediction for account creation "
                                     "password and username field.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME_AND_EMAIL_ADDRESS},
                  {.role = ElementRole::TYPE_TEXT_NEW_PASSWORD_FIELD,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              },
      },
      {
          .description_for_logging =
              "Server prediction for account creation password field only.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::TYPE_TEXT_NEW_PASSWORD_FIELD,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              },
      },
  });
}

// Checks that passwords of length one can only be saved on manual fallback.
TEST_F(FormParserTest, PasswordsWithLengthOneAreSavedOnlyOnManualFallback) {
  CheckTestData({{
      .description_for_logging =
          "Passwords of length 1 can be saved only on manual fallback.",
      .fields =
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .value = u"1",
               .form_control_type = FormControlType::kInputPassword,
               .predicted_type = autofill::PASSWORD},
              {.role = ElementRole::NEW_PASSWORD,
               .value = u"2",
               .form_control_type = FormControlType::kInputPassword,
               .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .value = u"3",
               .form_control_type = FormControlType::kInputPassword,
               .predicted_type = autofill::CONFIRMATION_PASSWORD},
              {.value = u"4",
               .form_control_type = FormControlType::kInputPassword},
          },
      .fallback_only = true,
  }});
}

TEST_F(FormParserTest, InferConfirmationPasswordField) {
  CheckTestData({
      {
          .description_for_logging = "Infer confirmation password during "
                                     "saving with server prediction.",
          .fields =
              {
                  {
                      .role = ElementRole::NEW_PASSWORD,
                      .value = u"pw",
                      .form_control_type = FormControlType::kInputPassword,
                      .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD,
                  },
                  {
                      .role_saving = ElementRole::CONFIRMATION_PASSWORD,
                      .value = u"pw",
                      .form_control_type = FormControlType::kInputPassword,
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
                      .form_control_type = FormControlType::kInputPassword,
                  },
                  {
                      .role_filling = ElementRole::NONE,
                      .role_saving = ElementRole::CONFIRMATION_PASSWORD,
                      .autocomplete_attribute = "off",
                      .value = u"pw",
                      .form_control_type = FormControlType::kInputPassword,
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
                      .role = ElementRole::NEW_PASSWORD,
                      .value = u"pw1",
                      .form_control_type = FormControlType::kInputPassword,
                      .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD,
                  },
                  {
                      .role_saving = ElementRole::NONE,
                      .value = u"pw2",
                      .form_control_type = FormControlType::kInputPassword,
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
                      .form_control_type = FormControlType::kInputPassword,
                  },
                  {
                      .role = ElementRole::NONE,
                      .value = u"pw2",
                      .form_control_type = FormControlType::kInputPassword,
                  },
              },
      },
  });
}

TEST_F(FormParserTest, ServerHints) {
  CheckTestData({
      {
          .description_for_logging = "Empty predictions don't cause panic",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Username-only predictions are not ignored",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging = "Simple predictions work",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME_AND_EMAIL_ADDRESS,
                   .may_use_prefilled_placeholder = true},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD,
                   .may_use_prefilled_placeholder = true},
              },
          .server_side_classification_successful = true,
          .username_may_use_prefilled_placeholder = true,
      },
      {
          .description_for_logging = "Longer predictions work",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CONFIRMATION_PASSWORD},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
          .number_of_all_alternative_passwords = 4,
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "password prediction for a non-password field is ignored",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::PASSWORD},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging = "Username not a placeholder",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME_AND_EMAIL_ADDRESS,
                   .may_use_prefilled_placeholder = false},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD,
                   .may_use_prefilled_placeholder = false},
              },
          .server_side_classification_successful = true,
          .username_may_use_prefilled_placeholder = false,
      },
  });
}

TEST_F(FormParserTest, Interactability) {
  CheckTestData({
      {
          .description_for_logging =
              "If all fields are hidden, all are considered",
          .fields =
              {
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::USERNAME,
                   .is_focusable = false,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "If some fields are hidden, only visible are considered",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 2,
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
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = true,
                   .form_control_type = FormControlType::kInputText},
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::kAutofilled,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::kUserTyped,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 3,
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
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .properties_mask = FieldPropertiesFlags::kAutofilled,
                   .form_control_type = FormControlType::kInputPassword},
                  {.is_focusable = true,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
              },
          .form_has_autofilled_value = true,
      },
      {
          .description_for_logging =
              "Interactability also matters for HTML classifier.",
          .fields =
              {
                  {.is_focusable = false,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_username = 0},
                  {.role = ElementRole::USERNAME,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_focusable = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
  });
}

TEST_F(FormParserTest, AllAlternativePasswords) {
  const AlternativeElementVector kPasswords = {
      {AlternativeElement::Value(u"password1"), autofill::FieldRendererId(10),
       AlternativeElement::Name(u"p1")},
      {AlternativeElement::Value(u"password2"), autofill::FieldRendererId(22),
       AlternativeElement::Name(u"p3")},
  };
  const AlternativeElementVector kUsernames = {
      {AlternativeElement::Value(u"username1"), autofill::FieldRendererId(12),
       AlternativeElement::Name(u"chosen")},
      {AlternativeElement::Value(u"username2"), autofill::FieldRendererId(17),
       AlternativeElement::Name(u"first")},
  };
  CheckTestData({
      {
          .description_for_logging = "It is always the first field name which "
                                     "is associated with a duplicated password "
                                     "value",
          .fields =
              {
                  {.value = kPasswords[0].value,
                   .name = kPasswords[0].name,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = kUsernames[0].value,
                   .name = kUsernames[0].name,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = kPasswords[0].value,
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = kUsernames[1].value,
                   .name = kUsernames[1].name,
                   .form_control_type = FormControlType::kInputText},
                  {.value = kUsernames[1].value,
                   .form_control_type = FormControlType::kInputText},
                  {.value = kPasswords[1].value,
                   .name = kPasswords[1].name,
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = kPasswords[1].value,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 2,
          .number_of_all_alternative_usernames = 2,
          .all_alternative_passwords = &kPasswords,
          .all_alternative_usernames = &kUsernames,
      },
      {
          .description_for_logging =
              "Empty values don't get added to all_alternative_passwords",
          .fields =
              {
                  {.value = u"",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role_filling = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
                  {.value = u"",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 0,
      },
      {
          .description_for_logging =
              "Empty values don't get added to "
              "all_alternative_passwords even if form gets "
              "parsed",
          .fields =
              {
                  {.value = u"",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
                  {.value = u"",
                   .form_control_type = FormControlType::kInputPassword},
                  {.value = u"",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 1,
      },
      {
          .description_for_logging =
              "A particular type of a squashed form (sign-in + sign-up)",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 3,
      },
      {
          .description_for_logging = "A strange but not squashed form",
          .fields =
              {
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputPassword},
                  {.form_control_type = FormControlType::kInputPassword},
              },
          .number_of_all_alternative_passwords = 4,
      },
  });
}

TEST_F(FormParserTest, UsernamePredictions) {
  CheckTestData({
      {
          .description_for_logging = "Username prediction overrides structure",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_username = 0},
                  {.form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText,
                   .predicted_username = 2},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Username prediction does not override autocomplete analysis",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_username = 0},
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Username prediction does not override server hints",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME_AND_EMAIL_ADDRESS},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_username = 0},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
      },
      {
          .description_for_logging = "Username prediction order matters",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_username = 1},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_username = 4},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
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
TEST_F(FormParserTest, ComplementingResults) {
  CheckTestData({
      {
          .description_for_logging = "Current password from autocomplete "
                                     "analysis, username from basic "
                                     "heuristics",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "New and confirmation passwords from server, username from basic "
              "heuristics",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CONFIRMATION_PASSWORD},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::NEW_PASSWORD},
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
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME_AND_EMAIL_ADDRESS},
                  {.form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
  });
}

// The parser should avoid identifying CVC fields as passwords.
TEST_F(FormParserTest, IgnoreCvcFields) {
  base::test::ScopedFeatureList feature_list;
  CheckTestData({
      {
          .description_for_logging =
              "Server hints: CREDIT_CARD_VERIFICATION_CODE.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CREDIT_CARD_VERIFICATION_CODE},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
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
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CREDIT_CARD_VERIFICATION_CODE},
              },
      },
      {
          .description_for_logging = "Name of 'verification_type' matches the "
                                     "CVC pattern, ignore that one.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.name = u"verification_type",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
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
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .name = u"verification_type",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = true,
      },
  });
}

TEST_F(FormParserTest, ServerHintsForCvcFieldsOverrideAutocomplete) {
  CheckTestData({
      {
          .description_for_logging =
              "Credit card server hints override autocomplete=*-password",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CREDIT_CARD_NUMBER},
                  {.autocomplete_attribute = "new-password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CREDIT_CARD_VERIFICATION_CODE},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hint turns autocomplete=cc-csc into a password field",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "cc-csc",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
          .fallback_only = false,
      },
  });
}

// The parser should avoid identifying Credit Card Number fields as passwords
// if the server identifies the fields as CC Number fields. This should be
// relatively safe as it should be unlikely that the server misclassifies a
// field as a CC Number field.
TEST_F(FormParserTest, CCNumber) {
  base::test::ScopedFeatureList feature_list;
  CheckTestData({
      {
          .description_for_logging = "Server hints: CREDIT_CARD_NUMBER.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CREDIT_CARD_NUMBER},
              },
      },
      {
          .description_for_logging =
              "Name of 'ccnumber' matches the CC Number regex pattern (but "
              "there is no confirmation from the server), ignore that one.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .name = u"ccnumber",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      // Server prediction `CREDIT_CARD_VERIFICATION_CODE` or
      // `CREDIT_CARD_NUMBER` on the password field must force Password Manager
      // to ignore the password field completely.
      {
          .description_for_logging = "Example where CC Number and Expiration "
                                     "date are both password fields.",
          .fields =
              {
                  {.name = u"cardholder",
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::CREDIT_CARD_NAME_FULL},
                  {.name = u"ccnumber",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CREDIT_CARD_NUMBER},
                  {.name = u"expiration",
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type =
                       autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
                  {.name = u"cvc",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CREDIT_CARD_VERIFICATION_CODE},
              },
      },
  });
}

TEST_F(FormParserTest, SSN_and_OTP) {
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
                     .form_control_type = FormControlType::kInputText},
                    {.name = field_name,
                     .form_control_type = FormControlType::kInputPassword},
                    {.role = ElementRole::CURRENT_PASSWORD,
                     .form_control_type = FormControlType::kInputPassword},
                },
            // The result should be trusted for more than just fallback, because
            // there is an actual password field present.
            .fallback_only = false,
        },
        {
            .description_for_logging = "Create a fallback for the only password"
                                       " field being an SSN/OTP field.",
            .fields =
                {
                    {.role = ElementRole::USERNAME,
                     .form_control_type = FormControlType::kInputText},
                    {.role = ElementRole::CURRENT_PASSWORD,
                     .name = field_name,
                     .form_control_type = FormControlType::kInputPassword},
                },
            .fallback_only = true,
        },
    });
  }
}

TEST_F(FormParserTest, OtpRegexMetric) {
  base::HistogramTester histogram_tester;
  CheckTestData({{
      .fields =
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = FormControlType::kInputText},
              {.name = u"OneTimePassword",
               .form_control_type = FormControlType::kInputPassword},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = FormControlType::kInputPassword},
          },
      .fallback_only = false,
  }});
  // Two samples because |CheckTestData| parses the form in two modes: filling
  // and saving.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ParserDetectedOtpFieldWithRegex", true, 2);
}

// The parser should avoid identifying NOT_PASSWORD fields as passwords.
TEST_F(FormParserTest, NotPasswordField) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_PASSWORD.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::NOT_PASSWORD},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_PASSWORD on only password.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::NOT_PASSWORD},
              },
      },
  });
}

// The parser should avoid identifying ONE_TIME_CODE fields as passwords.
TEST_F(FormParserTest, OneTimeCodeField) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: ONE_TIME_CODE.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ONE_TIME_CODE},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: ONE_TIME_CODE on only password.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ONE_TIME_CODE},
              },
      },
  });
}

// The parser should avoid identifying ONE_TIME_CODE fields as usernames.
TEST_F(FormParserTest, OneTimeCodeFieldNotUsername) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: ONE_TIME_CODE.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::ONE_TIME_CODE},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
  });
}

// The parser should avoid identifying NOT_USERNAME fields as usernames.
TEST_F(FormParserTest, NotUsernameField) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_USERNAME.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::NONE,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::NOT_USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_USERNAME on only username.",
          .fields =
              {
                  {.role = ElementRole::NONE,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::NOT_USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_USERNAME, despite 'predicted_username'.",
          .fields =
              {
                  {.role = ElementRole::NONE,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::NOT_USERNAME,
                   .predicted_username = 0},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
          .fallback_only = false,
      },
  });
}

// The parser should avoid identifying NOT_USERNAME fields as usernames despite
// autocomplete attribute.
TEST_F(FormParserTest, NotUsernameFieldDespiteAutocompelteAtrribute) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_USERNAME.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "username",
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::NOT_USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
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
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::NOT_USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
  });
}

// The parser should avoid identifying NOT_PASSWORD fields as passwords.
TEST_F(FormParserTest, NotPasswordFieldDespiteAutocompleteAttribute) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: NOT_PASSWORD.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::NOT_PASSWORD},
                  {.autocomplete_attribute = "new-password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::NOT_PASSWORD},
                  {.autocomplete_attribute = "password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::NOT_PASSWORD},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: NOT_PASSWORD on only password.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::NOT_PASSWORD},
              },
      },
  });
}

// The parser should avoid identifying ONE_TIME_CODE fields as passwords.
TEST_F(FormParserTest, OneTimeCodeFieldDespiteAutocompleteAttribute) {
  CheckTestData({
      {
          .description_for_logging = "Server hints: ONE_TIME_CODE.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ONE_TIME_CODE},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
      {
          .description_for_logging =
              "Server hints: ONE_TIME_CODE on only password.",
          .fields =
              {
                  {.form_control_type = FormControlType::kInputText},
                  {.autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ONE_TIME_CODE},
              },
      },
  });
}

// Check that "readonly status" is reported accordingly.
TEST_F(FormParserTest, ReadonlyStatus) {
  CheckTestData({
      {
          .description_for_logging =
              "Server predictions prevent heuristics from using readonly.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
      },
      {
          .description_for_logging =
              "Autocomplete attributes prevent heuristics from using readonly.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .autocomplete_attribute = "current-password",
                   .form_control_type = FormControlType::kInputPassword},
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
                  {.form_control_type = FormControlType::kInputText},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoHeuristics,
      },
      {
          .description_for_logging = "No readonly passwords ignored.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   // While readonly, this field is not ignored because it was
                   // autofilled before.
                   .is_readonly = true,
                   .properties_mask =
                       FieldPropertiesFlags::kAutofilledOnPageLoad,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .is_readonly = false,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kNoneIgnored,
          .form_has_autofilled_value = true,
      },
      {
          .description_for_logging = "Some readonly passwords ignored.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.is_readonly = true,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = false,
                   .form_control_type = FormControlType::kInputPassword},
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
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .is_readonly = true,
                   .form_control_type = FormControlType::kInputPassword},
              },
          .readonly_status =
              FormDataParser::ReadonlyPasswordFields::kAllIgnored,
          .fallback_only = true,
      },
  });
}

// Check that empty values are ignored when parsing for saving.
TEST_F(FormParserTest, NoEmptyValues) {
  CheckTestData({
      {
          .description_for_logging =
              "Server hints overridden for non-empty values.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role_saving = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
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
                   .form_control_type = FormControlType::kInputText},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "Structure heuristics overridden for non-empty values.",
          .fields =
              {
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText},
                  {.role_filling = ElementRole::USERNAME,
                   .value = u"",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword},
                  {.role_filling = ElementRole::NEW_PASSWORD,
                   .value = u"",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
  });
}

// Check that multiple usernames in server hints are handled properly.
TEST_F(FormParserTest, MultipleUsernames) {
  CheckTestData({
      {
          .description_for_logging = "More than two usernames are ignored.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging =
              "No current password -> ignore additional usernames.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              },
      },
      {
          .description_for_logging =
              "2 current passwods -> ignore additional usernames.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
                  {.role = ElementRole::NONE,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
      },
      {
          .description_for_logging =
              "No new password -> ignore additional usernames.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
      },
      {
          .description_for_logging = "Two usernames in sign-in, sign-up order.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              },
          .is_new_password_reliable = true,
      },
      {
          .description_for_logging = "Two usernames in sign-up, sign-in order.",
          .fields =
              {
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role_filling = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
              },
      },
      {
          .description_for_logging =
              "Two usernames in sign-in, sign-up order; sign-in is pre-filled.",
          .fields =
              {
                  {.role_filling = ElementRole::USERNAME,
                   .properties_mask =
                       FieldPropertiesFlags::kAutofilledOnPageLoad,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role_saving = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::PASSWORD},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              },
      },
  });
}

// If multiple hints for new-password fields are given (e.g., because of more
// fields having the same signature), the first one should be marked as
// new-password. That way the generation can be offered before the user has
// thought of and typed their new password elsewhere. See
// https://crbug.com/902700 for more details.
TEST_F(FormParserTest, MultipleNewPasswords) {
  CheckTestData({
      {
          .description_for_logging = "Only one new-password recognised.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
                  {.role = ElementRole::NONE,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
              },
      },
      {
          .description_for_logging =
              "Only one new-password recognised, confirmation unaffected.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::NEW_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
                  {.form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::ACCOUNT_CREATION_PASSWORD},
                  {.role = ElementRole::CONFIRMATION_PASSWORD,
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::CONFIRMATION_PASSWORD},
              },
      },
  });
}

TEST_F(FormParserTest, HistogramsForUsernameDetectionMethod) {
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
                       .form_control_type = FormControlType::kInputPassword,
                       .predicted_type = autofill::PASSWORD},
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
                       .form_control_type = FormControlType::kInputText,
                       .predicted_type = autofill::USERNAME},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .form_control_type = FormControlType::kInputPassword,
                       .predicted_type = autofill::PASSWORD},
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
                       .form_control_type = FormControlType::kInputText},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .autocomplete_attribute = "current-password",
                       .form_control_type = FormControlType::kInputPassword},
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
                       .form_control_type = FormControlType::kInputText,
                       .predicted_username = 0},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .form_control_type = FormControlType::kInputPassword},
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
                       .form_control_type = FormControlType::kInputText},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .form_control_type = FormControlType::kInputPassword},
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
                       .form_control_type = FormControlType::kInputText,
                       .predicted_username = 0},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .form_control_type = FormControlType::kInputPassword,
                       .predicted_type = autofill::PASSWORD},
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
                       .form_control_type = FormControlType::kInputText},
                      {.role = ElementRole::CURRENT_PASSWORD,
                       .autocomplete_attribute = "current-password",
                       .form_control_type = FormControlType::kInputPassword},
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

TEST_F(FormParserTest, SubmissionEvent) {
  CheckTestData({
      {.description_for_logging = "Sign-in form, submission event is not None",
       .fields =
           {
               {.role = ElementRole::USERNAME,
                .form_control_type = FormControlType::kInputText},
               {.role = ElementRole::CURRENT_PASSWORD,
                .form_control_type = FormControlType::kInputPassword},
           },
       .submission_event = SubmissionIndicatorEvent::XHR_SUCCEEDED},
  });
}

TEST_F(FormParserTest, GetSignonRealm) {
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

TEST_F(FormParserTest, TypedValues) {
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
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"js_password",
                   .user_input = u"typed_password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"js_new_password",
                   .user_input = u"typed_new_password",
                   .form_control_type = FormControlType::kInputPassword},
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
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
                   .user_input = u"typed_password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"",
                   .user_input = u"typed_new_password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .description_for_logging =
              "Form autocomplete with cleared by JavaScript values",
          // Username autocomplete tests that typed values are taken as
          // username, password and new password instead of values that are
          // cleared by JavaScript.
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .value = u"",
                   .user_input = u"typed_username",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .autocomplete_attribute = "current-password",
                   .value = u"",
                   .user_input = u"typed_password",
                   .form_control_type = FormControlType::kInputPassword},
                  {.role = ElementRole::NEW_PASSWORD,
                   .autocomplete_attribute = "new-password",
                   .value = u"",
                   .user_input = u"typed_new_password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
  });
}

TEST_F(FormParserTest, ContradictingPasswordPredictionAndAutocomplete) {
  CheckTestData({{
      .description_for_logging =
          "Server data and autocomplete contradict each other",
      // Server predictions have priority over autocomplete.
      .fields =
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .autocomplete_attribute = "new-password",
               .form_control_type = FormControlType::kInputPassword,
               .predicted_type = autofill::PASSWORD},
          },
  }});
}

TEST_F(FormParserTest, SingleUsernamePrediction) {
  CheckTestData({
      {
          .description_for_logging = "1 field",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::SINGLE_USERNAME},
              },
      },
      {
          .description_for_logging = "Field labeled as seach field",
          .fields =
              {
                  {.role = ElementRole::NONE,
                   .name = u"search_bar",
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::SINGLE_USERNAME},
              },
      },
      {
          .description_for_logging = "Nameless field",
          .fields =
              {
                  {.role = ElementRole::NONE,
                   .id_attribute = u"",
                   .name = u"",
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::SINGLE_USERNAME},
              },
      },
  });
}

// When the form has both user input in password fields and single
// username prediction, the later should be ignored  when the form is parsed
// for saving to avoid losing the password.
TEST_F(FormParserTest, BothSingleUsernameAndPassword) {
  CheckTestData({
      {
          .description_for_logging =
              "Form with a SINGLE_USERNAME prediction and "
              "user-typed password value.",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .user_input = u"typed_username",
                   .form_control_type = FormControlType::kInputText,
                   .predicted_type = autofill::SINGLE_USERNAME},
                  {.role_saving = ElementRole::CURRENT_PASSWORD,
                   .user_input = u"typed_password",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .fallback_only = false,
      },
  });
}

// Invalid form URLs should cause the parser to fail.
TEST_F(FormParserTest, InvalidURL) {
  FormParsingTestCase form_desc = {
      .fields =
          {
              {.form_control_type = FormControlType::kInputText},
              {.form_control_type = FormControlType::kInputPassword},
          },
  };
  FormPredictions no_predictions;
  ParseResultIds dummy;
  FormData form_data =
      GetFormDataAndExpectation(form_desc, &no_predictions, &dummy, &dummy);
  // URL comes from https://crbug.com/1075515.
  form_data.set_url(GURL("FilEsysteM:htTp:E=/."));
  FormDataParser parser;
  EXPECT_FALSE(parser.Parse(form_data, FormDataParser::Mode::kFilling,
                            /*stored_usernames=*/{}));
  EXPECT_FALSE(parser.Parse(form_data, FormDataParser::Mode::kSaving,
                            /*stored_usernames=*/{}));
}

TEST_F(FormParserTest, FindUsernameInPredictions_SkipPrediction) {
  // Searching username field should skip prediction that is less
  // likely to be user interaction. For example, if a field has no
  // user input while others have, the field cannot be an username
  // field.

  // Create a form containing username, email, id, password, submit.
  const FormParsingTestCase form_desc = {
      .fields = {
          {.name = u"username",
           .form_control_type = FormControlType::kInputText},
          {.name = u"email", .form_control_type = FormControlType::kInputText},
          {.name = u"id", .form_control_type = FormControlType::kInputText},
          {.name = u"password",
           .form_control_type = FormControlType::kInputPassword},
          {.name = u"submit", .form_control_type = FormControlType::kInputText},
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
  for (const auto& form_field_data : form_data.fields()) {
    processed_fields.push_back(ProcessedField{.field = &form_field_data});
  }

  processed_fields[2].interactability = Interactability::kCertain;  // id
  processed_fields[3].interactability = Interactability::kCertain;  // password

  // Add predictions for "email" and "id" fields. The "email" is in
  // front of "id", indicating "email" is more reliable.
  const std::vector<autofill::FieldRendererId> predictions = {
      form_data.fields()[1].renderer_id(),  // email
      form_data.fields()[2].renderer_id(),  // id
  };

  // Now search the username field. The username field is supposed to
  // be "id", not "email".
  const autofill::FormFieldData* field_data = FindUsernameInPredictions(
      predictions, processed_fields, Interactability::kCertain);
  ASSERT_TRUE(field_data);
  EXPECT_EQ(u"id", field_data->name());
}

// Tests that the form parser is not considering fields with values consisting
// of one repeated non alphanumeric symbol for saving.
TEST_F(FormParserTest, SkipHiddenValueField) {
  std::vector<FormParsingTestCase> test_cases = {
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = FormControlType::kInputText},
                  {.value = u"***********",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = FormControlType::kInputText},
                  {.value = u"**",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = FormControlType::kInputText},
                  {.value = u"••••••••",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      }};
  for (const auto& form_desc : test_cases) {
    FormPredictions no_predictions;
    ParseResultIds dummy;
    FormData form_data =
        GetFormDataAndExpectation(form_desc, &no_predictions, &dummy, &dummy);
    FormDataParser parser;
    EXPECT_TRUE(parser.Parse(form_data, FormDataParser::Mode::kFilling,
                             /*stored_usernames=*/{}));
    EXPECT_FALSE(parser.Parse(form_data, FormDataParser::Mode::kSaving,
                              /*stored_usernames=*/{}));
  }
}

// Tests that the form parser is not considering fields with values consisting
// of one repeated non alphanumeric symbol for saving.
TEST_F(FormParserTest, DontSkipNotHiddenValues) {
  std::vector<FormParsingTestCase> test_cases = {
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = FormControlType::kInputText},
                  {.value = u"a*******a",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = FormControlType::kInputText},
                  {.value = u".....*****",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
      {
          .fields =
              {
                  {.value = u"foo",
                   .name = u"username",
                   .form_control_type = FormControlType::kInputText},
                  {.value = u"0 0 0 0 0",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
      }};

  for (const auto& form_desc : test_cases) {
    FormPredictions no_predictions;
    ParseResultIds dummy;
    FormData form_data =
        GetFormDataAndExpectation(form_desc, &no_predictions, &dummy, &dummy);
    FormDataParser parser;
    EXPECT_TRUE(parser.Parse(form_data, FormDataParser::Mode::kFilling,
                             /*stored_usernames=*/{}));
    EXPECT_TRUE(parser.Parse(form_data, FormDataParser::Mode::kSaving,
                             /*stored_usernames=*/{}));
  }
}

// Tests that 'new-password' autocomplete attribute is ignored when two
// or more fields that have it have different values.
TEST_F(FormParserTest, AutocompleteAttributesError) {
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
                    .form_control_type = FormControlType::kInputPassword},
                   {.role_filling = ElementRole::CONFIRMATION_PASSWORD,
                    .role_saving = ElementRole::NEW_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"newpass",
                    .name = u"password2",
                    .form_control_type = FormControlType::kInputPassword},
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
                    .form_control_type = FormControlType::kInputPassword},
                   {.role_filling = ElementRole::CONFIRMATION_PASSWORD,
                    .role_saving = ElementRole::NEW_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"newpass",
                    .name = u"password2",
                    .form_control_type = FormControlType::kInputPassword},
                   {.role_saving = ElementRole::CONFIRMATION_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"newpass",
                    .name = u"password3",
                    .form_control_type = FormControlType::kInputPassword},
               },
       }});
}

// Tests that if the field is parsed as username based on server predictions,
// than it cannot be picked as password based on local heuristics.
TEST_F(FormParserTest, UsernameWithTypePasswordAndServerPredictions) {
  CheckTestData({
      {
          .description_for_logging =
              "Username with server predictions and type 'password'",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .value = u"testusername",
                   .name = u"field1",
                   .form_control_type = FormControlType::kInputPassword,
                   .predicted_type = autofill::USERNAME},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"testpass",
                   .name = u"field2",
                   .form_control_type = FormControlType::kInputPassword},
              },
      },
  });
}

// Tests that if a field is marked as autofill="webauthn" then the
// `accepts_webauthn_credentials` flag is set.
TEST_F(FormParserTest, AcceptsWebAuthnCredentials) {
  CheckTestData({
      {
          .description_for_logging = "Field tagged with autofill=\"webauthn\"",
          .fields =
              {
                  {.role = ElementRole::USERNAME,
                   .autocomplete_attribute = "webauthn",
                   .value = u"rosalina",
                   .name = u"username",
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"luma",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .accepts_webauthn_credentials = true,
      },
  });
}

// Tests that if there is a single field marked as autofill="webauthn" then the
// form is parsed and the `accepts_webauthn_credentials` flag is set.
// Regression test for crbug.com/1366006.
TEST_F(FormParserTest, SingleFieldAcceptsWebAuthnCredentials) {
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
                   .form_control_type = FormControlType::kInputText},
              },
          .accepts_webauthn_credentials = true,
      },
  });
}

// Tests that if a field is marked as autofill="username webauthn" then the
// `accepts_webauthn_credentials` flag is set.
TEST_F(FormParserTest, AcceptsUsernameWebAuthnCredentials) {
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
                   .form_control_type = FormControlType::kInputText},
                  {.role = ElementRole::CURRENT_PASSWORD,
                   .value = u"luma",
                   .name = u"password",
                   .form_control_type = FormControlType::kInputPassword},
              },
          .accepts_webauthn_credentials = true,
      },
  });
}

// Tests that if a username field was found by server prediction then the
// `username_detection_method` enum is correctly set.
TEST_F(FormParserTest, UsernameFoundByServerPredictions) {
  FormPredictions predictions;
  ParseResultIds fill_result;
  ParseResultIds save_result;
  FormParsingTestCase test_case = {
      .description_for_logging = "Username with server predictions",
      .fields = {{.role = ElementRole::USERNAME,
                  .form_control_type = FormControlType::kInputText,
                  .predicted_type = autofill::USERNAME},

                 {.role = ElementRole::CURRENT_PASSWORD,
                  .form_control_type = FormControlType::kInputPassword,
                  .predicted_type = autofill::PASSWORD}},
  };
  const FormData form_data = GetFormDataAndExpectation(
      {test_case}, &predictions, &fill_result, &save_result);
  FormDataParser parser;
  parser.set_predictions(std::move(predictions));

  auto [result, username_detection_method, is_new_password_reliable,
        suggestion_banned_fields, manual_generation_enabled_field] =
      parser.ParseAndReturnParsingResult(
          form_data, FormDataParser::Mode::kSaving, /*stored_usernames=*/{});
  EXPECT_EQ(username_detection_method,
            UsernameDetectionMethod::kServerSidePrediction);
}

TEST_F(FormParserTest, BaseHeuristicsFindUsernameFieldWithStoredUsername) {
  const std::u16string kUsername = u"the_username";
  FormData form_data;
  form_data.set_url(GURL("https://www.example.com"));
  form_data.set_fields({CreateField(FormControlType::kInputText, kUsername),
                        CreateField(FormControlType::kInputText, u""),
                        CreateField(FormControlType::kInputPassword, u"")});

  FormDataParser parser;
  auto [password_form, username_detection_method, is_new_password_reliable,
        suggestion_banned_fields, manual_generation_enabled_field] =
      parser.ParseAndReturnParsingResult(
          form_data, FormDataParser::Mode::kFilling, {kUsername});
  ASSERT_TRUE(password_form);

  EXPECT_EQ(username_detection_method, UsernameDetectionMethod::kBaseHeuristic);
  EXPECT_EQ(password_form->username_value, kUsername);
  EXPECT_TRUE(password_form->HasUsernameElement());
  EXPECT_EQ(password_form->username_element_renderer_id,
            form_data.fields()[0].renderer_id());
}

TEST_F(FormParserTest, PasswordFieldsWithMaxLength) {
  CheckTestData(
      {{
           .description_for_logging = "New password with maxlength attr "
                                      "sufficient for password generation",
           .fields =
               {
                   {.role = ElementRole::NEW_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"LoooongStrooongPassword",
                    .form_control_type = FormControlType::kInputPassword,
                    .max_length_attr = 13},
               },
           .is_new_password_reliable = true,
       },
       {
           .description_for_logging = "New password with small maxlength attr",
           .fields =
               {
                   {.role = ElementRole::NEW_PASSWORD,
                    .autocomplete_attribute = "new-password",
                    .value = u"Short",
                    .form_control_type = FormControlType::kInputPassword,
                    .max_length_attr = 5},
               },
           .is_new_password_reliable = false,
       }});
}

}  // namespace

}  // namespace password_manager
