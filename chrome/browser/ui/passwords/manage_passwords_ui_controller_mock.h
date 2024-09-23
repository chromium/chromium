// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/password_manager/core/browser/move_password_to_account_store_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}  // namespace content

class ManagePasswordsUIControllerMock : public ManagePasswordsUIController {
 public:
  explicit ManagePasswordsUIControllerMock(content::WebContents* contents);

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
  MOCK_METHOD(void, OnBubbleShown, (), (override));
  MOCK_METHOD(void, OnBubbleHidden, (), (override));
  MOCK_METHOD(void, OnNoInteraction, (), (override));
  MOCK_METHOD(void, OnNopeUpdateClicked, (), (override));
  MOCK_METHOD(void, NeverSavePassword, (), (override));
  MOCK_METHOD(void,
              SavePassword,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              ChooseCredential,
              (const password_manager::PasswordForm&,
               password_manager::CredentialType),
              (override));
  MOCK_METHOD(void,
              NavigateToPasswordManagerSettingsPage,
              (password_manager::ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(void, OnDialogHidden, (), (override));
  MOCK_METHOD(
      std::unique_ptr<password_manager::MovePasswordToAccountStoreHelper>,
      CreateMovePasswordToAccountStoreHelper,
      (const password_manager::PasswordForm&,
       password_manager::metrics_util::MoveToAccountStoreTrigger,
       base::OnceCallback<void()>),
      (override));
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_
