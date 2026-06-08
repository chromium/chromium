// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

using ::autofill::FormData;
using ::autofill::FormFieldData;
using ::password_manager::PasswordCredentialFillerImpl;
using ::password_manager::SubmissionReadinessState;
using ::testing ::_;
using ::testing::ReturnRefOfCopy;

constexpr char kExampleCom[] = "https://example.com/";
const std::u16string kUsername = u"coolUserName";
const std::u16string kPassword = u"383KySrSpR38";

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD(void,
              FillSuggestion,
              (const std::u16string&,
               const std::u16string&,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, TriggerFormSubmission, (), (override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const override));
  MOCK_METHOD(const url::Origin&, GetLastCommittedOrigin, (), (const override));
};

enum class FormFieldFocusabilityType {
  kFocusableInput,
  kFocusableCheckbox,
  kNonFocusableInput,
};

struct PasswordCredentialFillerV2ParameterTestParam {
  const std::vector<FormFieldFocusabilityType> focusability_vector;
  const bool has_captcha = false;
  const size_t username_field_index = 0;
  const size_t password_field_index = 0;
  const SubmissionReadinessState submission_readiness =
      SubmissionReadinessState::kNoInformation;
};

const std::vector<PasswordCredentialFillerV2ParameterTestParam>
    kPasswordCredentialFillerV2TestCases = {
        // Empty form data. Field IDs should not matter.
        {.focusability_vector = {},
         .has_captcha = false,
         .username_field_index = 0,
         .password_field_index = 0,
         .submission_readiness = SubmissionReadinessState::kError},
        {.focusability_vector = {FormFieldFocusabilityType::kNonFocusableInput,
                                 FormFieldFocusabilityType::kNonFocusableInput},
         .has_captcha = false,
         .username_field_index = 2,
         .password_field_index = 2,
         .submission_readiness = SubmissionReadinessState::kError},
        // There's no password field in this case, so expected
        // SubmissionReadiness is `kNoPasswordField`.
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput},
         .has_captcha = false,
         .username_field_index = 0,
         .password_field_index = 2,
         .submission_readiness = SubmissionReadinessState::kNoPasswordField},
        // There's no username field in this case, so expected
        // SubmissionReadiness is `kNoUsernameField`.
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput},
         .has_captcha = false,
         .username_field_index = 2,
         .password_field_index = 0,
         .submission_readiness = SubmissionReadinessState::kNoUsernameField},
        // There's a focusable field between username and password fields
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput},
         .has_captcha = false,
         .username_field_index = 0,
         .password_field_index = 2,
         .submission_readiness =
             SubmissionReadinessState::kFieldBetweenUsernameAndPassword},
        // There's an ignorable field between username and password fields. It's
        // doesn't matter if it's empty.
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kNonFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput},
         .has_captcha = false,
         .username_field_index = 0,
         .password_field_index = 2,
         .submission_readiness = SubmissionReadinessState::kTwoFields},
        // There's a focusable field after password field.
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput},
         .has_captcha = false,
         .username_field_index = 0,
         .password_field_index = 1,
         .submission_readiness =
             SubmissionReadinessState::kFieldAfterPasswordField},
        // There are unfocusable fields other than username and password fields.
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kNonFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kNonFocusableInput},
         .has_captcha = false,
         .username_field_index = 0,
         .password_field_index = 2,
         .submission_readiness = SubmissionReadinessState::kTwoFields},
        // There is a checkbox field after the password field.
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableCheckbox},
         .has_captcha = false,
         .username_field_index = 0,
         .password_field_index = 1,
         .submission_readiness = SubmissionReadinessState::kTwoFields},
        // There is a CAPTCHA within the form
        {.focusability_vector = {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput},
         .has_captcha = true,
         .username_field_index = 0,
         .password_field_index = 1,
         .submission_readiness = SubmissionReadinessState::kLikelyHasCaptcha},
};

class PasswordCredentialFillerBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(driver_, GetLastCommittedURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kExampleCom)));
    ON_CALL(driver_, GetLastCommittedOrigin())
        .WillByDefault(ReturnRefOfCopy(url::Origin::Create(GURL(kExampleCom))));
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  autofill::PasswordSuggestionRequest CreatePasswordSuggestionRequest(
      const std::vector<FormFieldFocusabilityType>& focusability_vector,
      bool has_captcha,
      size_t username_field_index,
      size_t password_field_index) {
    FormData form;
    form.set_host_frame(autofill::test::MakeLocalFrameToken());
    form.set_fields(base::ToVector(
        focusability_vector, [&](FormFieldFocusabilityType type) {
          FormFieldData field;
          field.set_host_frame(form.host_frame());
          field.set_renderer_id(autofill::test::MakeFieldRendererId());
          field.set_is_focusable(
              (type == FormFieldFocusabilityType::kFocusableInput ||
               type == FormFieldFocusabilityType::kFocusableCheckbox));
          field.set_form_control_type(
              (type == FormFieldFocusabilityType::kFocusableCheckbox)
                  ? autofill::FormControlType::kInputCheckbox
                  : autofill::FormControlType::kInputText);
          return field;
        }));
    // CAPTCHA is most often incapsulated into a separate iframe on the page. We
    // assume that if there is a child iframe in the password form, it is a
    // CAPTCHA.
    form.set_likely_contains_captcha(has_captcha);

    auto get_field_id = [&form](size_t i) {
      return autofill::FieldGlobalId{
          .frame_token = form.host_frame(),
          .renderer_id = i < form.fields().size()
                             ? form.fields()[i].renderer_id()
                             : autofill::FieldRendererId()};
    };
    return autofill::PasswordSuggestionRequest(
        autofill::TriggeringField(), form, get_field_id(username_field_index),
        get_field_id(password_field_index));
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  MockPasswordManagerDriver driver_;
};

TEST_F(PasswordCredentialFillerBaseTest, FillingFailed) {
  PasswordCredentialFillerImpl filler(
      driver().AsWeakPtr(), CreatePasswordSuggestionRequest(
                                {FormFieldFocusabilityType::kFocusableInput,
                                 FormFieldFocusabilityType::kFocusableInput},
                                /*has_captcha=*/false,
                                /*username_field_index=*/0,
                                /*password_field_index=*/1));

  ASSERT_EQ(filler.GetSubmissionReadinessState(),
            SubmissionReadinessState::kTwoFields);
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword, _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*was_filling_successful=*/false));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);
  base::MockCallback<base::OnceCallback<void(bool)>> reply_call;
  EXPECT_CALL(reply_call, Run(false));

  filler.FillUsernameAndPassword(kUsername, kPassword, reply_call.Get());
}

TEST_F(PasswordCredentialFillerBaseTest, FillWithNullDriver) {
  PasswordCredentialFillerImpl filler(
      nullptr, CreatePasswordSuggestionRequest({}, /*has_captcha=*/false,
                                               /*username_field_index=*/0,
                                               /*password_field_index=*/0));
  // Should not crash.
  filler.FillUsernameAndPassword(kUsername, kPassword, base::DoNothing());
}

TEST_F(PasswordCredentialFillerBaseTest, GetFrameOrigin) {
  PasswordCredentialFillerImpl filler(
      driver().AsWeakPtr(),
      CreatePasswordSuggestionRequest({}, /*has_captcha=*/false,
                                      /*username_field_index=*/0,
                                      /*password_field_index=*/0));
  EXPECT_EQ(filler.GetFrameOrigin(), url::Origin::Create(GURL(kExampleCom)));
}

TEST_F(PasswordCredentialFillerBaseTest, GetFrameOriginWithNullDriver) {
  PasswordCredentialFillerImpl filler(
      nullptr, CreatePasswordSuggestionRequest({}, /*has_captcha=*/false,
                                               /*username_field_index=*/0,
                                               /*password_field_index=*/0));
  EXPECT_TRUE(filler.GetFrameOrigin().opaque());
}

class PasswordCredentialFillerV2ParameterTest
    : public PasswordCredentialFillerBaseTest,
      public testing::WithParamInterface<
          PasswordCredentialFillerV2ParameterTestParam> {
 public:
  PasswordCredentialFillerImpl PrepareFiller() {
    return PasswordCredentialFillerImpl(
        driver().AsWeakPtr(),
        CreatePasswordSuggestionRequest(
            GetParam().focusability_vector, GetParam().has_captcha,
            GetParam().username_field_index, GetParam().password_field_index));
  }
};

TEST_P(PasswordCredentialFillerV2ParameterTest, FillWithUsername) {
  SubmissionReadinessState submission_readiness =
      GetParam().submission_readiness;

  // If there is no field after the password and both username and password
  // fields are there, then submit the form.
  bool submission_expected =
      submission_readiness == SubmissionReadinessState::kEmptyFields ||
      submission_readiness == SubmissionReadinessState::kMoreThanTwoFields ||
      submission_readiness == SubmissionReadinessState::kTwoFields;

  PasswordCredentialFillerImpl filler = PrepareFiller();

  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword, _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*was_filling_successful=*/true));
  EXPECT_CALL(driver(), TriggerFormSubmission)
      .Times(submission_expected ? 1 : 0);

  filler.FillUsernameAndPassword(kUsername, kPassword, base::DoNothing());
}

TEST_P(PasswordCredentialFillerV2ParameterTest, FillWithEmptyUsername) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  const std::u16string kEmptyUsername = u"";

  EXPECT_CALL(driver(), FillSuggestion(kEmptyUsername, kPassword, _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*was_filling_successful=*/true));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.FillUsernameAndPassword(kEmptyUsername, kPassword, base::DoNothing());
}

TEST_P(PasswordCredentialFillerV2ParameterTest,
       UpdateTriggerSubmissionWithTrueControlsFormSubmission) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  // override SubmissionReadiness by calling UpdateTriggerSubmission:
  filler.UpdateTriggerSubmission(true);
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword, _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*was_filling_successful=*/true));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(1);

  filler.FillUsernameAndPassword(kUsername, kPassword, base::DoNothing());
}

TEST_P(PasswordCredentialFillerV2ParameterTest,
       UpdateTriggerSubmissionWithFalseControlsFormSubmission) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  // override SubmissionReadiness by calling UpdateTriggerSubmission:
  filler.UpdateTriggerSubmission(false);
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword, _))
      .WillOnce(
          base::test::RunOnceCallback<2>(/*was_filling_successful=*/true));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.FillUsernameAndPassword(kUsername, kPassword, base::DoNothing());
}

TEST_P(PasswordCredentialFillerV2ParameterTest, SubmissionReadiness) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  EXPECT_EQ(filler.GetSubmissionReadinessState(),
            GetParam().submission_readiness);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordCredentialFillerV2ParameterTest,
    testing::ValuesIn(kPasswordCredentialFillerV2TestCases));

}  // namespace
