// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::FormData;
using autofill::mojom::SubmissionReadinessState;
using ToShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ToShowVirtualKeyboard;
using password_manager::PasswordCredentialFillerImpl;
using password_manager::SubmissionReadinessParams;
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
    const std::vector<FormFieldFocusabilityType>& focusability_vector) {
  FormData form;
  base::ranges::transform(
      focusability_vector, std::back_inserter(form.fields),
      [](FormFieldFocusabilityType type) {
        autofill::FormFieldData field;
        field.is_focusable =
            (type == FormFieldFocusabilityType::kFocusableInput ||
             type == FormFieldFocusabilityType::kFocusableCheckbox);
        field.form_control_type =
            (type == FormFieldFocusabilityType::kFocusableCheckbox)
                ? autofill::FormControlType::kInputCheckbox
                : autofill::FormControlType::kEmpty;
        return field;
      });
  return form;
}

const std::vector<
    std::tuple<SubmissionReadinessParams, SubmissionReadinessState>>
    kPasswordCredentialFillerV2TestCases = {
        // empty form data. field indices should not matter.
        {SubmissionReadinessParams(PrepareFormData({}),
                                   /*username_field_index=*/0,
                                   /*password_field_index=*/0,
                                   SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kError},
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput}),
             /*username_field_index=*/2,
             /*password_field_index=*/2,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kError},
        // There's no password field in this case, so expected
        // SubmissionReadiness is `kNoPasswordField`.
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput}),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kNoPasswordField},
        // There's no username field in this case, so expected
        // SubmissionReadiness is `kNoUsernameField`.
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput}),
             /*username_field_index=*/2,
             /*password_field_index=*/0,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kNoUsernameField},
        // There's a focusable field between username and password fields
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput}),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kFieldBetweenUsernameAndPassword},
        // There's an ignorable field between username and password fields. It's
        // doesn't matter if it's empty.
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput}),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kTwoFields},
        // There's a focusable field after password field.
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput}),
             /*username_field_index=*/0,
             /*password_field_index=*/1,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kFieldAfterPasswordField},
        // There are unfocusable fields other than username and password fields.
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kNonFocusableInput}),
             /*username_field_index=*/0,
             /*password_field_index=*/2,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kTwoFields},
        // There is a checkbox field after the password field.
        {SubmissionReadinessParams(
             PrepareFormData({FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableInput,
                              FormFieldFocusabilityType::kFocusableCheckbox}),
             /*username_field_index=*/0,
             /*password_field_index=*/1,
             SubmissionReadinessState::kNoInformation),
         SubmissionReadinessState::kTwoFields},
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
        SubmissionReadinessParams(form, 0, 0, GetParam()));
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
          std::tuple<SubmissionReadinessParams, SubmissionReadinessState>> {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      password_manager::features::kPasswordSuggestionBottomSheetV2};
};

TEST_P(PasswordCredentialFillerV2Test, SubmissionReadiness) {
  SubmissionReadinessParams params = std::get<0>(GetParam());

  PasswordCredentialFillerImpl filler(driver().AsWeakPtr(), params);
  EXPECT_EQ(filler.GetSubmissionReadinessState(), std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PasswordCredentialFillerV2Test,
    testing::ValuesIn(kPasswordCredentialFillerV2TestCases));
