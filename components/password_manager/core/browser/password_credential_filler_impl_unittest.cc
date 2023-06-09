// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::mojom::SubmissionReadinessState;
using ToShowVirtualKeyboard =
    password_manager::PasswordManagerDriver::ToShowVirtualKeyboard;
using password_manager::PasswordCredentialFillerImpl;
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

}  // namespace

class PasswordCredentialFillerTest
    : public testing::TestWithParam<SubmissionReadinessState> {
 protected:
  void SetUp() override {
    ON_CALL(driver_, GetLastCommittedURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kExampleCom)));
  }

  MockPasswordManagerDriver& driver() { return driver_; }

 private:
  MockPasswordManagerDriver driver_;
};

TEST_P(PasswordCredentialFillerTest, FillWithUsername) {
  SubmissionReadinessState submission_readiness = GetParam();

  // If there is no field after the password and both username and password
  // fields are there, then submit the form.
  bool submission_expected =
      submission_readiness == SubmissionReadinessState::kEmptyFields ||
      submission_readiness == SubmissionReadinessState::kMoreThanTwoFields ||
      submission_readiness == SubmissionReadinessState::kTwoFields;

  PasswordCredentialFillerImpl filler(driver().AsWeakPtr(),
                                      submission_readiness);

  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission)
      .Times(submission_expected ? 1 : 0);

  filler.FillUsernameAndPassword(kUsername, kPassword);
}

TEST_P(PasswordCredentialFillerTest, FillWithEmptyUsername) {
  PasswordCredentialFillerImpl filler(driver().AsWeakPtr(), GetParam());
  const std::u16string kEmptyUsername = u"";

  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kEmptyUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.FillUsernameAndPassword(kEmptyUsername, kPassword);

  ASSERT_FALSE(filler.IsReadyToFill());
}

TEST_P(PasswordCredentialFillerTest,
       UpdateTriggerSubmissionWithTrueControlsFormSubmission) {
  PasswordCredentialFillerImpl filler(driver().AsWeakPtr(), GetParam());
  // override SubmissionReadiness by calling UpdateTriggerSubmission:
  filler.UpdateTriggerSubmission(true);
  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(1);

  filler.FillUsernameAndPassword(kUsername, kPassword);

  // FillUsernameAndPassword can be called only once.
  EXPECT_FALSE(filler.IsReadyToFill());
}

TEST_P(PasswordCredentialFillerTest,
       UpdateTriggerSubmissionWithFalseControlsFormSubmission) {
  PasswordCredentialFillerImpl filler(driver().AsWeakPtr(), GetParam());
  // override SubmissionReadiness by calling UpdateTriggerSubmission:
  filler.UpdateTriggerSubmission(false);
  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), FillSuggestion(kUsername, kPassword));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.FillUsernameAndPassword(kUsername, kPassword);

  // FillUsernameAndPassword can be called only once.
  EXPECT_FALSE(filler.IsReadyToFill());
}

TEST_P(PasswordCredentialFillerTest, CleanUp) {
  PasswordCredentialFillerImpl filler(driver().AsWeakPtr(), GetParam());
  EXPECT_CALL(driver(),
              KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false)));
  EXPECT_CALL(driver(), TriggerFormSubmission).Times(0);

  filler.CleanUp(ToShowVirtualKeyboard(false));

  // FillUsernameAndPassword or CleanUp can be called only once.
  EXPECT_FALSE(filler.IsReadyToFill());
}

INSTANTIATE_TEST_SUITE_P(
    SubmissionReadinessVariation,
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
