// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"

#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
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
using ToShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ToShowVirtualKeyboard;
using password_manager::PasswordCredentialFillerImpl;
using password_manager::PasswordFillingParams;
using testing::ReturnRefOfCopy;

constexpr char kExampleCom[] = "https://example.com/";
const std::u16string kUsername = u"coolUserName";
const std::u16string kPassword = u"383KySrSpR38";

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD(void,
              FillSuggestion,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              KeyboardReplacingSurfaceClosed,
              (ToShowVirtualKeyboard),
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
  base::ranges::transform(
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

const std::vector<std::tuple<PasswordFillingParams, SubmissionReadinessState>>
    kPasswordCredentialFillerV2TestCases = {
        // empty form data. field indices should not matter.
        {PasswordFillingParams(
             PrepareFormData({}, /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/0,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kError},
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/2,
             /*password_field_index=*/2,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kError},
        // There's no password field in this case, so expected
        // SubmissionReadiness is `kNoPasswordField`.
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kNoPasswordField},
        // There's no username field in this case, so expected
        // SubmissionReadiness is `kNoUsernameField`.
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/2,
             /*password_field_index=*/0,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kNoUsernameField},
        // There's a focusable field between username and password fields
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kFieldBetweenUsernameAndPassword},
        // There's an ignorable field between username and password fields. It's
        // doesn't matter if it's empty.
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kTwoFields},
        // There's a focusable field after password field.
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/1,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kFieldAfterPasswordField},
        // There are unfocusable fields other than username and password fields.
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kTwoFields},
        // There is a checkbox field after the password field.
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableCheckbox},
                             /*has_captcha=*/false),
             /*username_field_index=*/0,
             /*password_field_index=*/1,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kTwoFields},
        // There is a CAPTCHA within the form
        {PasswordFillingParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput},
                             /*has_captcha=*/true),
             /*username_field_index=*/0,
             /*password_field_index=*/1,
             /*focused_field_renderer_id_=*/autofill::FieldRendererId(),
             SubmissionReadinessState::kNoInformation),
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

class PasswordCredentialFillerTest
    : public PasswordCredentialFillerBaseTest,
      public testing::WithParamInterface<SubmissionReadinessState> {
 public:
  PasswordCredentialFillerImpl PrepareFiller() {
    const FormData form;
    return PasswordCredentialFillerImpl(
        driver().AsWeakPtr(),
        PasswordFillingParams(form, 0, 0, autofill::FieldRendererId(),
                              GetParam()));
  }
};

TEST_P(PasswordCredentialFillerTest, FillWithUsername) {
  SubmissionReadinessState submission_readiness = GetParam();

  // If there is no field after the password and both username and password
  // fields are there, then submit the form.
  bool submission_expected =
      submission_readiness == SubmissionReadinessState::kEmptyFields ||
      submission_readiness == SubmissionReadinessState::kMoreThanTwoFields ||
      submission_readiness == SubmissionReadinessState::kTwoFields;

  PasswordCredentialFillerImpl filler = PrepareFiller();

  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission)
      .Times(submission_expected ? 1 : 0);

  filler.FillUsernameAndPassword(kUsername, kPassword);
}

TEST_P(PasswordCredentialFillerTest, FillWithEmptyUsername) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  const std::u16string kEmptyUsername = u"";

  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kEmptyUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.FillUsernameAndPassword(kEmptyUsername, kPassword);
}

TEST_P(PasswordCredentialFillerTest,
       UpdateTriggerSubmissionWithTrueControlsFormSubmission) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  // override SubmissionReadiness by calling UpdateTriggerSubmission:
  filler.UpdateTriggerSubmission(true);
  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(1);

  filler.FillUsernameAndPassword(kUsername, kPassword);
}

TEST_P(PasswordCredentialFillerTest,
       UpdateTriggerSubmissionWithFalseControlsFormSubmission) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  // override SubmissionReadiness by calling UpdateTriggerSubmission:
  filler.UpdateTriggerSubmission(false);
  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.FillUsernameAndPassword(kUsername, kPassword);
}

TEST_P(PasswordCredentialFillerTest, FillWithNullDriver) {
  PasswordCredentialFillerImpl filler(
      nullptr, PasswordFillingParams(FormData(), 0, 0,
                                     autofill::FieldRendererId(), GetParam()));
  // Should not crash.
  filler.FillUsernameAndPassword(kUsername, kPassword);
}

TEST_P(PasswordCredentialFillerTest, Dismiss) {
  PasswordCredentialFillerImpl filler = PrepareFiller();
  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.Dismiss(ToShowVirtualKeyboard(false));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordCredentialFillerTest,
    testing::Values(SubmissionReadinessState::kNoInformation,
                    SubmissionReadinessState::kError,
                    SubmissionReadinessState::kNoUsernameField,
                    SubmissionReadinessState::kFieldBetweenUsernameAndPassword,
                    SubmissionReadinessState::kFieldAfterPasswordField,
                    SubmissionReadinessState::kEmptyFields,
                    SubmissionReadinessState::kMoreThanTwoFields,
                    SubmissionReadinessState::kTwoFields,
                    SubmissionReadinessState::kNoPasswordField));

class PasswordCredentialFillerV2Test
    : public PasswordCredentialFillerBaseTest,
      public testing::WithParamInterface<
          std::tuple<PasswordFillingParams, SubmissionReadinessState>> {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      password_manager::features::kPasswordSuggestionBottomSheetV2};
};

TEST_P(PasswordCredentialFillerV2Test, SubmissionReadiness) {
  PasswordFillingParams params = std::get<0>(GetParam());

  PasswordCredentialFillerImpl filler(driver().AsWeakPtr(), params);
  EXPECT_EQ(filler.GetSubmissionReadinessState(), std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordCredentialFillerV2Test,
    testing::ValuesIn(kPasswordCredentialFillerV2TestCases));
