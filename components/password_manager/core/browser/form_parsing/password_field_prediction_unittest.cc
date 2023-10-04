// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::ACCOUNT_CREATION_PASSWORD;
using autofill::AutofillType;
using autofill::CalculateFieldSignatureForField;
using autofill::CalculateFormSignature;
using autofill::CONFIRMATION_PASSWORD;
using autofill::CREDIT_CARD_VERIFICATION_CODE;
using autofill::EMAIL_ADDRESS;
using autofill::FieldGlobalId;
using autofill::FormControlType;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::NEW_PASSWORD;
using autofill::NO_SERVER_DATA;
using autofill::PASSWORD;
using autofill::ServerFieldType;
using autofill::SINGLE_USERNAME;
using autofill::UNKNOWN_TYPE;
using autofill::USERNAME;
using autofill::USERNAME_AND_EMAIL_ADDRESS;
using base::ASCIIToUTF16;

using FieldPrediction = autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;
using ::autofill::test::CreateFieldPrediction;

namespace password_manager {

namespace {

TEST(FormPredictionsTest, ConvertToFormPredictions) {
  struct TestField {
    std::string name;
    FormControlType form_control_type;
    ServerFieldType input_type;
    ServerFieldType expected_type;
    bool may_use_prefilled_placeholder;
    std::vector<ServerFieldType> additional_types;
  } test_fields[] = {
      {"full_name", FormControlType::kInputText, UNKNOWN_TYPE, UNKNOWN_TYPE,
       false},
      // Password Manager is interested only in credential related types.
      {"Email", FormControlType::kInputEmail, EMAIL_ADDRESS, EMAIL_ADDRESS,
       false},
      {"username", FormControlType::kInputText, USERNAME, USERNAME, true},
      {"Password", FormControlType::kInputPassword, PASSWORD, PASSWORD, false},
      {"confirm_password", FormControlType::kInputPassword,
       CONFIRMATION_PASSWORD, CONFIRMATION_PASSWORD, true},
      // username in |additional_types| takes precedence if the feature is
      // enabled.
      {"email",
       FormControlType::kInputText,
       EMAIL_ADDRESS,
       USERNAME,
       false,
       {USERNAME}},
      // cvc in |additional_types| takes precedence if the feature is enabled.
      {"cvc",
       FormControlType::kInputPassword,
       PASSWORD,
       CREDIT_CARD_VERIFICATION_CODE,
       false,
       {CREDIT_CARD_VERIFICATION_CODE}},
      // non-password, non-cvc types in |additional_types| are ignored.
      {"email",
       FormControlType::kInputText,
       UNKNOWN_TYPE,
       UNKNOWN_TYPE,
       false,
       {EMAIL_ADDRESS}},
  };

  FormData form_data;
  base::flat_map<FieldGlobalId, AutofillType::ServerPrediction>
      autofill_predictions;
  for (size_t i = 0; i < std::size(test_fields); ++i) {
    FormFieldData field;
    field.unique_renderer_id = autofill::FieldRendererId(i + 1000);
    field.name = ASCIIToUTF16(test_fields[i].name);
    field.form_control_type = test_fields[i].form_control_type;

    AutofillType::ServerPrediction prediction;
    prediction.server_predictions.push_back(
        CreateFieldPrediction(test_fields[i].input_type));
    for (ServerFieldType type : test_fields[i].additional_types) {
      prediction.server_predictions.push_back(CreateFieldPrediction(type));
    }
    prediction.may_use_prefilled_placeholder =
        test_fields[i].may_use_prefilled_placeholder;
    autofill_predictions.insert({field.global_id(), std::move(prediction)});
    form_data.fields.push_back(std::move(field));
  }

  constexpr int driver_id = 1000;
  FormPredictions actual_predictions =
      ConvertToFormPredictions(driver_id, form_data, autofill_predictions);

  // Check whether actual predictions are equal to expected ones.
  EXPECT_EQ(driver_id, actual_predictions.driver_id);
  EXPECT_EQ(CalculateFormSignature(form_data),
            actual_predictions.form_signature);
  EXPECT_EQ(std::size(test_fields), actual_predictions.fields.size());

  for (size_t i = 0; i < std::size(test_fields); ++i) {
    const PasswordFieldPrediction& actual_prediction =
        actual_predictions.fields[i];
    EXPECT_EQ(test_fields[i].expected_type, actual_prediction.type);
    EXPECT_EQ(test_fields[i].may_use_prefilled_placeholder,
              actual_prediction.may_use_prefilled_placeholder);
    EXPECT_EQ(CalculateFieldSignatureForField(form_data.fields[i]),
              actual_prediction.signature);
  }
}

TEST(FormPredictionsTest, ConvertToFormPredictions_SynthesiseConfirmation) {
  struct TestField {
    std::string name;
    FormControlType form_control_type;
    ServerFieldType input_type;
    ServerFieldType expected_type;
  };
  const std::vector<TestField> kTestForms[] = {
      {
          {"username", FormControlType::kInputText, USERNAME, USERNAME},
          {"new password", FormControlType::kInputPassword,
           ACCOUNT_CREATION_PASSWORD, ACCOUNT_CREATION_PASSWORD},
          // Same name and type means same signature. As a second new-password
          // field with this signature, the next field should be re-classified
          // to confirmation password.
          {"new password", FormControlType::kInputPassword,
           ACCOUNT_CREATION_PASSWORD, CONFIRMATION_PASSWORD},
      },
      {
          {"username", FormControlType::kInputText, USERNAME, USERNAME},
          {"new password duplicate", FormControlType::kInputPassword,
           ACCOUNT_CREATION_PASSWORD, ACCOUNT_CREATION_PASSWORD},
          // An explicit confirmation password above should override the
          // 2-new-passwords heuristic.
          {"new password duplicate", FormControlType::kInputPassword,
           ACCOUNT_CREATION_PASSWORD, ACCOUNT_CREATION_PASSWORD},
          {"confirm_password", FormControlType::kInputPassword,
           CONFIRMATION_PASSWORD, CONFIRMATION_PASSWORD},
      },
  };

  for (const std::vector<TestField>& test_form : kTestForms) {
    FormData form_data;
    base::flat_map<FieldGlobalId, AutofillType::ServerPrediction>
        autofill_predictions;
    for (size_t i = 0; i < test_form.size(); ++i) {
      FormFieldData field;
      field.unique_renderer_id = autofill::FieldRendererId(i + 1000);
      field.name = ASCIIToUTF16(test_form[i].name);
      field.form_control_type = test_form[i].form_control_type;

      AutofillType::ServerPrediction new_prediction;
      new_prediction.server_predictions = {
          CreateFieldPrediction(test_form[i].input_type)};
      autofill_predictions.insert(
          {field.global_id(), std::move(new_prediction)});

      form_data.fields.push_back(std::move(field));
    }

    FormPredictions actual_predictions = ConvertToFormPredictions(
        /*driver_id=*/0, form_data, autofill_predictions);

    for (size_t i = 0; i < form_data.fields.size(); ++i) {
      SCOPED_TRACE(
          testing::Message()
          << "field description: name=" << test_form[i].name
          << ", form control type="
          << autofill::FormControlTypeToString(test_form[i].form_control_type)
          << ", input type=" << test_form[i].input_type
          << ", expected type=" << test_form[i].expected_type
          << ", synthesised FormFieldData=" << form_data.fields[i]);
      EXPECT_EQ(test_form[i].expected_type, actual_predictions.fields[i].type);
    }
  }
}

TEST(FormPredictionsTest, DeriveFromServerFieldType) {
  struct TestCase {
    const char* name;
    // Input.
    ServerFieldType server_type;
    CredentialFieldType expected_result;
  } test_cases[] = {
      {"No prediction", NO_SERVER_DATA, CredentialFieldType::kNone},
      {"Irrelevant type", EMAIL_ADDRESS, CredentialFieldType::kNone},
      {"Username", USERNAME, CredentialFieldType::kUsername},
      {"Username/Email", USERNAME_AND_EMAIL_ADDRESS,
       CredentialFieldType::kUsername},
      {"Single Username", SINGLE_USERNAME,
       CredentialFieldType::kSingleUsername},
      {"Password", PASSWORD, CredentialFieldType::kCurrentPassword},
      {"New password", NEW_PASSWORD, CredentialFieldType::kNewPassword},
      {"Account creation password", ACCOUNT_CREATION_PASSWORD,
       CredentialFieldType::kNewPassword},
      {"Confirmation password", CONFIRMATION_PASSWORD,
       CredentialFieldType::kConfirmationPassword},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EXPECT_EQ(test_case.expected_result,
              DeriveFromServerFieldType(test_case.server_type));
  }
}

// Tests that if |AutofillType::ServerPrediction| has an override flag, it
// will be propagated to |FormPredictions|.
TEST(FormPredictionsTest, ConvertToFormPredictions_OverrideFlagPropagated) {
  constexpr int driver_id = 0;

  FormData form;
  FormFieldData single_username_field;
  single_username_field.unique_renderer_id = autofill::FieldRendererId(1000);
  form.fields.push_back(single_username_field);

  base::flat_map<FieldGlobalId, AutofillType::ServerPrediction>
      autofill_predictions;
  AutofillType::ServerPrediction autofill_prediction;
  autofill_prediction.server_predictions.push_back(
      CreateFieldPrediction(autofill::SINGLE_USERNAME, /*is_override=*/true));
  autofill_predictions.insert(
      {single_username_field.global_id(), autofill_prediction});

  FormPredictions expected_result;
  expected_result.driver_id = driver_id;
  expected_result.form_signature = CalculateFormSignature(form);
  expected_result.fields.push_back(
      {single_username_field.unique_renderer_id,
       CalculateFieldSignatureForField(single_username_field),
       autofill::SINGLE_USERNAME, /*may_use_prefilled_placeholder=*/false,
       /*is_override=*/true});

  EXPECT_EQ(ConvertToFormPredictions(driver_id, form, autofill_predictions),
            expected_result);
}

}  // namespace

}  // namespace password_manager
