// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_MOCK_H_

#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class CredentialManagerDialogControllerMock
    : public CredentialManagerDialogController {
 public:
  CredentialManagerDialogControllerMock();

  CredentialManagerDialogControllerMock(
      const CredentialManagerDialogControllerMock&) = delete;
  CredentialManagerDialogControllerMock& operator=(
      const CredentialManagerDialogControllerMock&) = delete;

  ~CredentialManagerDialogControllerMock() override;

  MOCK_METHOD(const FormsVector&, GetLocalForms, (), (const, override));
  MOCK_METHOD(url::Origin, GetOrigin, (), (const, override));
  MOCK_METHOD(bool, IsShowingAccountChooser, (), (const, override));
  MOCK_METHOD(std::u16string, GetAccountChooserTitle, (), (const, override));
  MOCK_METHOD(bool, ShouldShowSignInButton, (), (const, override));
  MOCK_METHOD(std::u16string, GetAutoSigninPromoTitle, (), (const, override));
  MOCK_METHOD(std::u16string, GetAutoSigninText, (), (const, override));
  MOCK_METHOD(bool, ShouldShowFooter, (), (const, override));
  MOCK_METHOD(void,
              OnChooseCredentials,
              (const password_manager::PasswordForm& password_form,
               password_manager::CredentialType credential_type),
              (override));
  MOCK_METHOD(void, OnSignInClicked, (), (override));
  MOCK_METHOD(void, OnAutoSigninOK, (), (override));
  MOCK_METHOD(void, OnAutoSigninTurnOff, (), (override));
  MOCK_METHOD(void, OnCloseDialog, (), (override));

  MOCK_METHOD(DisplayType, GetDisplayType, (), (const, override));
  MOCK_METHOD(bool, ShouldShowTopIllustration, (), (const, override));
  MOCK_METHOD(std::u16string, GetTitle, (), (const, override));
  MOCK_METHOD(std::u16string, GetSubtitle, (), (const, override));
  MOCK_METHOD(std::u16string, GetOkButtonLabel, (), (const, override));
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_MOCK_H_
