// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_COMMON_SAVED_ACCOUNT_MANAGER_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_COMMON_SAVED_ACCOUNT_MANAGER_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"

// This controller provides common logic for bubbles that are used to add/update
// credentials.
class CommonSavedAccountManagerBubbleController
    : public PasswordBubbleControllerBase {
 public:
  CommonSavedAccountManagerBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      DisplayReason display_reason,
      password_manager::metrics_util::UIDisplayDisposition display_disposition);
  ~CommonSavedAccountManagerBubbleController() override;

  // Called by the view code when the "NoThanks" button in clicked by the user.
  void OnNoThanksClicked();

  // Called by the view code when username or password is corrected using
  // the username correction or password selection features in PendingView.
  void OnCredentialEdited(std::u16string new_username,
                          std::u16string new_password);

  // Called by the view code when the "Google Password Manager" link in the
  // bubble footer in clicked by the user.
  void OnGooglePasswordManagerLinkClicked(
      password_manager::ManagePasswordsReferrer refferer);

  // Returns the email of current primary account. Returns empty string if no
  // account is signed in.
  std::u16string GetPrimaryAccountEmail();

  const password_manager::PasswordForm& pending_password() const {
    return pending_password_;
  }

  password_manager::ui::State state() const { return state_; }

  password_manager::metrics_util::UIDismissalReason GetDismissalReason() const;

  // Invoked upon the conclusion of the os authentication flow. Invokes
  // `completion` with the `authentication_result`.
  void OnUserAuthenticationCompleted(base::OnceCallback<void(bool)> completion,
                                     bool authentication_result);

 protected:
  url::Origin GetOrigin() const;
  password_manager::ui::State GetState() const;
  const password_manager::PasswordForm& GetPendingPassword() const;
  password_manager::metrics_util::UIDisplayDisposition GetDisplayDisposition()
      const;
  void SetDismissalReason(
      password_manager::metrics_util::UIDismissalReason reason);

 private:
  // Origin of the page from where this bubble was triggered.
  url::Origin origin_;

  password_manager::ui::State state_;
  password_manager::PasswordForm pending_password_;
  const password_manager::metrics_util::UIDisplayDisposition
      display_disposition_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_COMMON_SAVED_ACCOUNT_MANAGER_BUBBLE_CONTROLLER_H_
