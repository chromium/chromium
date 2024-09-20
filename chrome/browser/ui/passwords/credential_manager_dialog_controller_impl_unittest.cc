// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_manager_dialog_controller_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::StrictMock;

const char16_t kUsername[] = u"user1";
const char16_t kUsername2[] = u"user2";

class MockPasswordPrompt : public AccountChooserPrompt,
                           public AutoSigninFirstRunPrompt {
 public:
  MockPasswordPrompt() = default;

  MockPasswordPrompt(const MockPasswordPrompt&) = delete;
  MockPasswordPrompt& operator=(const MockPasswordPrompt&) = delete;

  MOCK_METHOD(void, ShowAccountChooser, (), (override));
  MOCK_METHOD(void, ShowAutoSigninPrompt, (), (override));
  MOCK_METHOD(void, ControllerGone, (), (override));
};

password_manager::PasswordForm GetLocalForm() {
  password_manager::PasswordForm form;
  form.username_value = kUsername;
  form.url = GURL("http://example.com");
  return form;
}

class CredentialManagerDialogControllerTest : public testing::Test {
 public:
  CredentialManagerDialogControllerTest()
      : controller_(&profile_, &ui_controller_mock_) {}

  void SetUp() override {
    ON_CALL(ui_controller_mock_, GetPasswordFeatureManager)
        .WillByDefault(testing::Return(&feature_manager_));
  }

  password_manager::MockPasswordFeatureManager& feature_manager() {
    return feature_manager_;
  }

  PasswordsModelDelegateMock& ui_controller_mock() {
    return ui_controller_mock_;
  }

  CredentialManagerDialogControllerImpl& controller() { return controller_; }

  PrefService* prefs() { return profile_.GetPrefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  testing::NiceMock<password_manager::MockPasswordFeatureManager>
      feature_manager_;
  testing::NiceMock<PasswordsModelDelegateMock> ui_controller_mock_;
  CredentialManagerDialogControllerImpl controller_;
};

TEST_F(CredentialManagerDialogControllerTest, ShowAccountChooser) {
  StrictMock<MockPasswordPrompt> prompt;
  password_manager::PasswordForm local_form = GetLocalForm();
  password_manager::PasswordForm local_form2 = local_form;
  local_form2.username_value = kUsername2;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> locals;
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(local_form));
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(local_form2));
  password_manager::PasswordForm* local_form_ptr = locals[0].get();

  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt, std::move(locals));
  EXPECT_THAT(controller().GetLocalForms(),
              ElementsAre(Pointee(local_form), Pointee(local_form2)));
  EXPECT_FALSE(controller().ShouldShowSignInButton());

  // Close the dialog.
  EXPECT_CALL(prompt, ControllerGone());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_CALL(feature_manager(), IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(testing::Return(false));
#endif
  EXPECT_CALL(ui_controller_mock(),
              ChooseCredential(
                  local_form,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  controller().OnChooseCredentials(
      *local_form_ptr,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerDialogControllerTest, ShowAccountChooserAndSignIn) {
  StrictMock<MockPasswordPrompt> prompt;
  password_manager::PasswordForm local_form = GetLocalForm();
  std::vector<std::unique_ptr<password_manager::PasswordForm>> locals;
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(local_form));

  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt, std::move(locals));
  EXPECT_THAT(controller().GetLocalForms(), ElementsAre(Pointee(local_form)));
  EXPECT_TRUE(controller().ShouldShowSignInButton());

  // Close the dialog.
  EXPECT_CALL(prompt, ControllerGone());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_CALL(feature_manager(), IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(testing::Return(false));
#endif
  EXPECT_CALL(ui_controller_mock(),
              ChooseCredential(
                  local_form,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  controller().OnSignInClicked();
}

TEST_F(CredentialManagerDialogControllerTest, AccountChooserClosed) {
  StrictMock<MockPasswordPrompt> prompt;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> locals;
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(GetLocalForm()));
  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt, std::move(locals));

  EXPECT_CALL(ui_controller_mock(), OnDialogHidden());
  controller().OnCloseDialog();
}

TEST_F(CredentialManagerDialogControllerTest, AutoSigninPromo) {
  base::HistogramTester histogram_tester;
  StrictMock<MockPasswordPrompt> prompt;
  EXPECT_CALL(prompt, ShowAutoSigninPrompt());
  controller().ShowAutosigninPrompt(&prompt);

  prefs()->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
  EXPECT_CALL(ui_controller_mock(), OnDialogHidden());
  controller().OnCloseDialog();
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          prefs()));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AutoSigninFirstRunDialog",
      password_manager::metrics_util::AUTO_SIGNIN_NO_ACTION, 1);
}

TEST_F(CredentialManagerDialogControllerTest, AutoSigninPromoOkGotIt) {
  base::HistogramTester histogram_tester;
  StrictMock<MockPasswordPrompt> prompt;
  EXPECT_CALL(prompt, ShowAutoSigninPrompt());
  controller().ShowAutosigninPrompt(&prompt);

  prefs()->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
  prefs()->SetBoolean(password_manager::prefs::kCredentialsEnableAutosignin,
                      true);
  EXPECT_CALL(prompt, ControllerGone());
  EXPECT_CALL(ui_controller_mock(), OnDialogHidden());
  controller().OnAutoSigninOK();
  EXPECT_FALSE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          prefs()));
  EXPECT_TRUE(prefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AutoSigninFirstRunDialog",
      password_manager::metrics_util::AUTO_SIGNIN_OK_GOT_IT, 1);
}

TEST_F(CredentialManagerDialogControllerTest, AutoSigninPromoTurnOff) {
  base::HistogramTester histogram_tester;
  StrictMock<MockPasswordPrompt> prompt;
  EXPECT_CALL(prompt, ShowAutoSigninPrompt());
  controller().ShowAutosigninPrompt(&prompt);

  prefs()->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, false);
  prefs()->SetBoolean(password_manager::prefs::kCredentialsEnableAutosignin,
                      true);
  EXPECT_CALL(prompt, ControllerGone());
  EXPECT_CALL(ui_controller_mock(), OnDialogHidden());
  controller().OnAutoSigninTurnOff();
  EXPECT_FALSE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          prefs()));
  EXPECT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AutoSigninFirstRunDialog",
      password_manager::metrics_util::AUTO_SIGNIN_TURN_OFF, 1);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(CredentialManagerDialogControllerTest, SignInBiometricsEnabled) {
  StrictMock<MockPasswordPrompt> prompt;
  password_manager::PasswordForm local_form = GetLocalForm();
  std::vector<std::unique_ptr<password_manager::PasswordForm>> locals;
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(local_form));

  EXPECT_CALL(prompt, ShowAccountChooser);
  controller().ShowAccountChooser(&prompt, std::move(locals));

  EXPECT_CALL(feature_manager(), IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(ui_controller_mock(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>([](auto callback) {
        // Simulate successful authentication.
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(prompt, ControllerGone);
  EXPECT_CALL(ui_controller_mock(),
              ChooseCredential(
                  local_form,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  controller().OnSignInClicked();
}

TEST_F(CredentialManagerDialogControllerTest,
       SignInBiometricsEnabledButFailed) {
  StrictMock<MockPasswordPrompt> prompt;
  password_manager::PasswordForm local_form = GetLocalForm();
  std::vector<std::unique_ptr<password_manager::PasswordForm>> locals;
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(local_form));

  EXPECT_CALL(prompt, ShowAccountChooser);
  controller().ShowAccountChooser(&prompt, std::move(locals));

  EXPECT_CALL(feature_manager(), IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(ui_controller_mock(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>([](auto callback) {
        // Simulate failed authentication.
        std::move(callback).Run(false);
      }));
  EXPECT_CALL(prompt, ControllerGone).Times(0);
  EXPECT_CALL(ui_controller_mock(), ChooseCredential).Times(0);
  controller().OnSignInClicked();

  testing::Mock::VerifyAndClearExpectations(&prompt);
  testing::Mock::VerifyAndClearExpectations(&ui_controller_mock());

  controller().OnCloseDialog();
}

TEST_F(CredentialManagerDialogControllerTest,
       OnChooseCredentialsBiometricsEnabled) {
  StrictMock<MockPasswordPrompt> prompt;

  EXPECT_CALL(prompt, ShowAccountChooser);
  std::vector<std::unique_ptr<password_manager::PasswordForm>> locals;
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(GetLocalForm()));
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(GetLocalForm()));
  controller().ShowAccountChooser(&prompt, std::move(locals));

  EXPECT_CALL(feature_manager(), IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(ui_controller_mock(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>([](auto callback) {
        // Simulate successful authentication.
        std::move(callback).Run(true);
      }));
  EXPECT_CALL(prompt, ControllerGone);
  EXPECT_CALL(ui_controller_mock(),
              ChooseCredential(
                  GetLocalForm(),
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  controller().OnChooseCredentials(
      GetLocalForm(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
}

TEST_F(CredentialManagerDialogControllerTest,
       OnChooseCredentialsBiometricsEnabledButFailed) {
  StrictMock<MockPasswordPrompt> prompt;

  EXPECT_CALL(prompt, ShowAccountChooser);
  std::vector<std::unique_ptr<password_manager::PasswordForm>> locals;
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(GetLocalForm()));
  locals.push_back(
      std::make_unique<password_manager::PasswordForm>(GetLocalForm()));
  controller().ShowAccountChooser(&prompt, std::move(locals));

  EXPECT_CALL(feature_manager(), IsBiometricAuthenticationBeforeFillingEnabled)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(ui_controller_mock(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>([](auto callback) {
        // Simulate failed authentication.
        std::move(callback).Run(false);
      }));
  EXPECT_CALL(prompt, ControllerGone).Times(0);
  EXPECT_CALL(ui_controller_mock(), ChooseCredential).Times(0);
  controller().OnChooseCredentials(
      GetLocalForm(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  testing::Mock::VerifyAndClearExpectations(&prompt);
  testing::Mock::VerifyAndClearExpectations(&ui_controller_mock());

  controller().OnCloseDialog();
}
#endif

}  // namespace
