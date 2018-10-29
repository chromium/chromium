// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class PasswordsModelDelegateMock
    : public PasswordsModelDelegate,
      public base::SupportsWeakPtr<PasswordsModelDelegateMock>{
 public:
  PasswordsModelDelegateMock();
  ~PasswordsModelDelegateMock() override;

  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD0(GetPasswordFormMetricsRecorder,
               password_manager::PasswordFormMetricsRecorder*());
  MOCK_CONST_METHOD0(GetOrigin, const GURL&());
  MOCK_CONST_METHOD0(GetState, password_manager::ui::State());
  MOCK_CONST_METHOD0(GetPendingPassword, const autofill::PasswordForm&());
  MOCK_CONST_METHOD0(GetCredentialSource,
                     password_manager::metrics_util::CredentialSourceType());
  MOCK_CONST_METHOD0(
      GetCurrentForms,
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&());
  MOCK_CONST_METHOD0(GetCurrentInteractionStats,
                     password_manager::InteractionsStats*());
  MOCK_CONST_METHOD0(BubbleIsManualFallbackForSaving, bool());
  MOCK_METHOD0(OnBubbleShown, void());
  MOCK_METHOD0(OnBubbleHidden, void());
  MOCK_METHOD0(OnNoInteraction, void());
  MOCK_METHOD0(OnNopeUpdateClicked, void());
  MOCK_METHOD0(NeverSavePassword, void());
  MOCK_METHOD1(UpdatePassword, void(const autofill::PasswordForm&));
  MOCK_METHOD0(OnPasswordsRevealed, void());
  MOCK_METHOD2(SavePassword,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD2(ChooseCredential, void(const autofill::PasswordForm&,
                                      password_manager::CredentialType));
  MOCK_METHOD0(NavigateToPasswordManagerAccountDashboard, void());
  MOCK_METHOD0(NavigateToPasswordManagerSettingsPage, void());
  MOCK_METHOD2(EnableSync,
               void(const AccountInfo& account, bool is_default_promo_account));
  MOCK_METHOD0(OnDialogHidden, void());
  MOCK_METHOD0(AuthenticateUser, bool());
  MOCK_CONST_METHOD0(ArePasswordsRevealedWhenBubbleIsOpened, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsModelDelegateMock);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_
