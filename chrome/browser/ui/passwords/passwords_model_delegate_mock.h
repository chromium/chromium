// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"

class PasswordsModelDelegateMock : public PasswordsModelDelegate {
 public:
  PasswordsModelDelegateMock();

  PasswordsModelDelegateMock(const PasswordsModelDelegateMock&) = delete;
  PasswordsModelDelegateMock& operator=(const PasswordsModelDelegateMock&) =
      delete;

  ~PasswordsModelDelegateMock() override;

  MOCK_METHOD(content::WebContents*, GetWebContents, (), (const override));
  MOCK_METHOD(password_manager::PasswordFormMetricsRecorder*,
              GetPasswordFormMetricsRecorder,
              (),
              (override));
  MOCK_METHOD(password_manager::PasswordFeatureManager*,
              GetPasswordFeatureManager,
              (),
              (override));
  MOCK_METHOD(url::Origin, GetOrigin, (), (const override));
  MOCK_METHOD(password_manager::ui::State, GetState, (), (const override));
  MOCK_METHOD(const password_manager::PasswordForm&,
              GetPendingPassword,
              (),
              (const override));
  MOCK_METHOD(const std::vector<password_manager::PasswordForm>&,
              GetUnsyncedCredentials,
              (),
              (const override));
  MOCK_METHOD(password_manager::metrics_util::CredentialSourceType,
              GetCredentialSource,
              (),
              (const override));
  MOCK_METHOD(
      const std::vector<std::unique_ptr<password_manager::PasswordForm>>&,
      GetCurrentForms,
      (),
      (const override));
  MOCK_METHOD(const std::optional<password_manager::PasswordForm>&,
              GetManagePasswordsSingleCredentialDetailsModeCredential,
              (),
              (const override));
  MOCK_METHOD(password_manager::InteractionsStats*,
              GetCurrentInteractionStats,
              (),
              (const override));
  MOCK_METHOD(size_t, GetTotalNumberCompromisedPasswords, (), (const override));
  MOCK_METHOD(bool, DidAuthForAccountStoreOptInFail, (), (const override));
  MOCK_METHOD(bool, BubbleIsManualFallbackForSaving, (), (const override));
  MOCK_METHOD(bool,
              GpmPinCreatedDuringRecentPasskeyCreation,
              (),
              (const override));
  MOCK_METHOD(void, OnBubbleShown, (), (override));
  MOCK_METHOD(void, OnBubbleHidden, (), (override));
  MOCK_METHOD(void, OnNoInteraction, (), (override));
  MOCK_METHOD(void, OnNopeUpdateClicked, (), (override));
  MOCK_METHOD(void, NeverSavePassword, (), (override));
  MOCK_METHOD(void, OnPasswordsRevealed, (), (override));
  MOCK_METHOD(void,
              SavePassword,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              SaveUnsyncedCredentialsInProfileStore,
              (const std::vector<password_manager::PasswordForm>&),
              (override));
  MOCK_METHOD(void, DiscardUnsyncedCredentials, (), (override));
  MOCK_METHOD(void, MovePasswordToAccountStore, (), (override));
  MOCK_METHOD(void, BlockMovingPasswordToAccountStore, (), (override));
  MOCK_METHOD(void, PromptSaveBubbleAfterDefaultStoreChanged, (), (override));
  MOCK_METHOD(void,
              ChooseCredential,
              (const password_manager::PasswordForm&,
               password_manager::CredentialType),
              (override));
  MOCK_METHOD(void,
              NavigateToPasswordManagerSettingsPage,
              (password_manager::ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(void,
              NavigateToPasswordDetailsPageInPasswordManager,
              (const std::string&, password_manager::ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(void,
              NavigateToPasswordManagerSettingsAccountStoreToggle,
              (password_manager::ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(void,
              NavigateToPasswordCheckup,
              (password_manager::PasswordCheckReferrer),
              (override));
  MOCK_METHOD(void,
              MovePendingPasswordToAccountStoreUsingHelper,
              (const password_manager::PasswordForm&,
               password_manager::metrics_util::MoveToAccountStoreTrigger),
              (override));
  MOCK_METHOD(void, OnDialogHidden, (), (override));
  MOCK_METHOD(void,
              AuthenticateUserWithMessage,
              (const std::u16string& message, AvailabilityCallback callback),
              (override));
  MOCK_METHOD(void,
              AuthenticateUserForAccountStoreOptInAndSavePassword,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(
      void,
      AuthenticateUserForAccountStoreOptInAfterSavingLocallyAndMovePassword,
      (),
      (override));
  MOCK_METHOD(void, ShowBiometricActivationConfirmation, (), (override));
  MOCK_METHOD(void,
              ShowMovePasswordBubble,
              (const password_manager::PasswordForm& form),
              (override));
  MOCK_METHOD(void, OnBiometricAuthBeforeFillingDeclined, (), (override));
  MOCK_METHOD(void,
              OnAddUsernameSaveClicked,
              (const std::u16string&, const password_manager::PasswordForm&),
              (override));
  MOCK_METHOD(void, MaybeShowIOSPasswordPromo, (), (override));
  MOCK_METHOD(void, RelaunchChrome, (), (override));

  base::WeakPtr<PasswordsModelDelegateMock> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<PasswordsModelDelegateMock> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_
