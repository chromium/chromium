// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
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

const char kUsername[] = "user1";
const char kUsername2[] = "user2";

class MockPasswordPrompt : public AccountChooserPrompt,
                           public AutoSigninFirstRunPrompt {
 public:
  MockPasswordPrompt() = default;

  MOCK_METHOD0(ShowAccountChooser, void());
  MOCK_METHOD0(ShowAutoSigninPrompt, void());
  MOCK_METHOD0(ControllerGone, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordPrompt);
};

autofill::PasswordForm GetLocalForm() {
  autofill::PasswordForm form;
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.origin = GURL("http://example.com");
  return form;
}

class CredentialManagerDialogControllerTest : public testing::Test {
 public:
  CredentialManagerDialogControllerTest()
      : controller_(&profile_, &ui_controller_mock_) {}

  PasswordsModelDelegateMock& ui_controller_mock() {
    return ui_controller_mock_;
  }

  CredentialManagerDialogControllerImpl& controller() { return controller_; }

  PrefService* prefs() { return profile_.GetPrefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  StrictMock<PasswordsModelDelegateMock> ui_controller_mock_;
  CredentialManagerDialogControllerImpl controller_;
};

TEST_F(CredentialManagerDialogControllerTest, ShowAccountChooser) {
  base::HistogramTester histogram_tester;
  StrictMock<MockPasswordPrompt> prompt;
  autofill::PasswordForm local_form = GetLocalForm();
  autofill::PasswordForm local_form2 = local_form;
  local_form2.username_value = base::ASCIIToUTF16(kUsername2);
  std::vector<std::unique_ptr<autofill::PasswordForm>> locals;
  locals.push_back(std::make_unique<autofill::PasswordForm>(local_form));
  locals.push_back(std::make_unique<autofill::PasswordForm>(local_form2));
  autofill::PasswordForm* local_form_ptr = locals[0].get();

  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt, std::move(locals));
  EXPECT_THAT(controller().GetLocalForms(),
              ElementsAre(Pointee(local_form), Pointee(local_form2)));
  EXPECT_FALSE(controller().ShouldShowSignInButton());

  // Close the dialog.
  EXPECT_CALL(prompt, ControllerGone());
  EXPECT_CALL(ui_controller_mock(),
              ChooseCredential(
                  local_form,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  controller().OnChooseCredentials(
      *local_form_ptr,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogMultipleAccounts",
      password_manager::metrics_util::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogOneAccount", 0);
}

TEST_F(CredentialManagerDialogControllerTest, ShowAccountChooserAndSignIn) {
  base::HistogramTester histogram_tester;
  StrictMock<MockPasswordPrompt> prompt;
  autofill::PasswordForm local_form = GetLocalForm();
  std::vector<std::unique_ptr<autofill::PasswordForm>> locals;
  locals.push_back(std::make_unique<autofill::PasswordForm>(local_form));

  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt, std::move(locals));
  EXPECT_THAT(controller().GetLocalForms(), ElementsAre(Pointee(local_form)));
  EXPECT_TRUE(controller().ShouldShowSignInButton());

  // Close the dialog.
  EXPECT_CALL(prompt, ControllerGone());
  EXPECT_CALL(ui_controller_mock(),
              ChooseCredential(
                  local_form,
                  password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  controller().OnSignInClicked();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogOneAccount",
      password_manager::metrics_util::ACCOUNT_CHOOSER_SIGN_IN, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogMultipleAccounts", 0);
}

TEST_F(CredentialManagerDialogControllerTest, AccountChooserClosed) {
  base::HistogramTester histogram_tester;
  StrictMock<MockPasswordPrompt> prompt;
  std::vector<std::unique_ptr<autofill::PasswordForm>> locals;
  locals.push_back(std::make_unique<autofill::PasswordForm>(GetLocalForm()));
  EXPECT_CALL(prompt, ShowAccountChooser());
  controller().ShowAccountChooser(&prompt, std::move(locals));

  EXPECT_CALL(ui_controller_mock(), OnDialogHidden());
  controller().OnCloseDialog();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountChooserDialogOneAccount",
      password_manager::metrics_util::ACCOUNT_CHOOSER_DISMISSED, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AccountChooserDialogMultipleAccounts", 0);
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

}  // namespace
