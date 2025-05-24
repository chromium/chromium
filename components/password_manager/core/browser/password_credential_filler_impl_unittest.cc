// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"

#include <algorithm>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
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

namespace {

using autofill::FormData;
using autofill::FormFieldData;
using autofill::mojom::SubmissionReadinessState;
using password_manager::PasswordCredentialFillerImpl;
using testing ::_;
using testing::ReturnRefOfCopy;

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
};

enum class FormFieldFocusabilityType {
  kFocusableInput,
  kFocusableCheckbox,
  kNonFocusableInput,
};

const FormData PrepareFormData(
    const std::vector<FormFieldFocusabilityType>& focusability_vector,
    bool has_captcha) {
  std::vector<FormFieldData> fields;
  std::ranges::transform(
      focusability_vector, std::back_inserter(fields),
      [](FormFieldFocusabilityType type) {
        FormFieldData field;
        field.set_is_focusable(
            (type == FormFieldFocusabilityType::kFocusableInput ||
             type == FormFieldFocusabilityType::kFocusableCheckbox));
        field.set_form_control_type(
            (type == FormFieldFocusabilityType::kFocusableCheckbox)
                ? autofill::FormControlType::kInputCheckbox
                : autofill::FormControlType::kInputText);
        return field;
      });
  FormData form;
  form.set_fields(std::move(fields));
  // CAPTCHA is most often incapsulated into a separate iframe on the page. We
  // assume that if there is a child iframe in the password form, it is a
  // CAPTCHA.
  if (has_captcha) {
    form.set_likely_contains_captcha(true);
  }
  return form;
}

const std::vector<
    std::tuple<autofill::PasswordSuggestionRequest, SubmissionReadinessState>>
    kPasswordCredentialFillerV2TestCases = {
        // empty form data. field indices should not matter.
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({}, /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/0),
         SubmissionReadinessState::kError},
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/2,
             /*password_field_index=*/2),
         SubmissionReadinessState::kError},
        // There's no password field in this case, so expected
        // SubmissionReadiness is `kNoPasswordField`.
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2),
         SubmissionReadinessState::kNoPasswordField},
        // There's no username field in this case, so expected
        // SubmissionReadiness is `kNoUsernameField`.
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/2,
             /*password_field_index=*/0),
         SubmissionReadinessState::kNoUsernameField},
        // There's a focusable field between username and password fields
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2),
         SubmissionReadinessState::kFieldBetweenUsernameAndPassword},
        // There's an ignorable field between username and password fields. It's
        // doesn't matter if it's empty.
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2),
         SubmissionReadinessState::kTwoFields},
        // There's a focusable field after password field.
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/1),
         SubmissionReadinessState::kFieldAfterPasswordField},
        // There are unfocusable fields other than username and password fields.
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2),
         SubmissionReadinessState::kTwoFields},
        // There is a checkbox field after the password field.
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableCheckbox},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/1),
         SubmissionReadinessState::kTwoFields},
        // There is a CAPTCHA within the form
        {autofill::PasswordSuggestionRequest(
             autofill::TriggeringField(),
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/true),
             /*username_field_index=*/0,
             /*password_field_index=*/1),
         SubmissionReadinessState::kLikelyHasCaptcha},
};

}  // namespace

class PasswordCredentialFillerBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(driver_, GetLastCommittedURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kExampleCom)));
  }

  MockPasswordManagerDriver& driver() { return driver_; }

 private:
  MockPasswordManagerDriver driver_;
};

TEST_F(PasswordCredentialFillerBaseTest, FillingFailed) {
  PasswordCredentialFillerImpl filler(
      driver().AsWeakPtr(),
      autofill::PasswordSuggestionRequest(
          autofill::TriggeringField(),
          PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                           FormFieldFocusabilityType::kFocusableInput},
                          /*has_captcha=*/false),
          /*username_field_index=*/0,
          /*password_field_index=*/1));

  ASSERT_EQ(filler.GetSubmissionReadinessState(),
            autofill::mojom::SubmissionReadinessState::kTwoFields);
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
      nullptr, autofill::PasswordSuggestionRequest(autofill::TriggeringField(),
                                                   FormData(), 0, 0));
  // Should not crash.
  filler.FillUsernameAndPassword(kUsername, kPassword, base::DoNothing());
}

class PasswordCredentialFillerV2ParameterTest
    : public PasswordCredentialFillerBaseTest,
      public testing::WithParamInterface<
          std::tuple<autofill::PasswordSuggestionRequest,
                     SubmissionReadinessState>> {
 public:
  PasswordCredentialFillerImpl PrepareFiller() {
    autofill::PasswordSuggestionRequest request = std::get<0>(GetParam());
    return PasswordCredentialFillerImpl(driver().AsWeakPtr(), request);
  }

 private:
};

TEST_P(PasswordCredentialFillerV2ParameterTest, FillWithUsername) {
  SubmissionReadinessState submission_readiness = std::get<1>(GetParam());

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
  EXPECT_EQ(filler.GetSubmissionReadinessState(), std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordCredentialFillerV2ParameterTest,
    testing::ValuesIn(kPasswordCredentialFillerV2TestCases));
