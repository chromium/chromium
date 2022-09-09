// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}  // namespace content

class ManagePasswordsUIControllerMock : public ManagePasswordsUIController {
 public:
  explicit ManagePasswordsUIControllerMock(
      content::WebContents* contents);

  ManagePasswordsUIControllerMock(const ManagePasswordsUIControllerMock&) =
      delete;
  ManagePasswordsUIControllerMock& operator=(
      const ManagePasswordsUIControllerMock&) = delete;

  ~ManagePasswordsUIControllerMock() override;

  MOCK_CONST_METHOD0(GetOrigin, url::Origin());
  MOCK_CONST_METHOD0(GetState, password_manager::ui::State());
  MOCK_CONST_METHOD0(GetPendingPassword,
                     const password_manager::PasswordForm&());
  MOCK_CONST_METHOD0(IsPasswordOverridden, bool());
  MOCK_CONST_METHOD0(
      GetCurrentForms,
      const std::vector<std::unique_ptr<password_manager::PasswordForm>>&());
  MOCK_CONST_METHOD0(GetCurrentInteractionStats,
                     password_manager::InteractionsStats*());
  MOCK_METHOD0(OnBubbleShown, void());
  MOCK_METHOD0(OnBubbleHidden, void());
  MOCK_METHOD0(OnNoInteraction, void());
  MOCK_METHOD0(OnNopeUpdateClicked, void());
  MOCK_METHOD0(NeverSavePassword, void());
  MOCK_METHOD1(UpdatePassword, void(const password_manager::PasswordForm&));
  MOCK_METHOD2(SavePassword,
               void(const std::u16string&, const std::u16string&));
  MOCK_METHOD2(ChooseCredential,
               void(const password_manager::PasswordForm&,
                    password_manager::CredentialType));
  MOCK_METHOD1(NavigateToPasswordManagerSettingsPage,
               void(password_manager::ManagePasswordsReferrer));
  MOCK_METHOD0(NavigateToChromeSignIn, void());
  MOCK_METHOD0(OnDialogHidden, void());
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_
