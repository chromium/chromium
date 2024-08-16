// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/gfx/image/image.h"

class PasswordsModelDelegate;

namespace favicon_base {
struct FaviconImageResult;
}

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

// This controller provides data and actions for the ManagePasswordsView.
class ManagePasswordsBubbleController : public PasswordBubbleControllerBase {
 public:
  enum class SyncState {
    kNotActive,
    kActiveWithSyncFeatureEnabled,
    kActiveWithAccountPasswords,
  };

  // This bubble is used to display either:
  // 1. List of credentials stored in the password store for the current origin.
  // 2. Details of a specific credential.
  // This is decided by the value returned by `PasswordsModelDelegate`s
  // `GetManagePasswordSingleCredentialModeCredential()`.
  enum class BubbleMode { kCredentialList, kSingleCredentialDetails };

  explicit ManagePasswordsBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~ManagePasswordsBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;

  // Called by the view code when the manage button is clicked by the user.
  void OnManageClicked(password_manager::ManagePasswordsReferrer referrer);

  // Called by the view code when the user clicks the "Manage button" to open
  // the details page for a particular credential.
  void OnManagePasswordClicked(
      password_manager::ManagePasswordsReferrer referrer);

  // Makes a request to the favicon service for the icon of current visible URL.
  // The request to the favicon store is canceled on destruction of the
  // controller.
  void RequestFavicon(
      base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback);

  SyncState GetPasswordSyncState() const;

  // Returns the email of current primary account. Returns empty string if no
  // account is signed in.
  std::u16string GetPrimaryAccountEmail();

  // Called by the view code when the "Google Password Manager" link in the
  // bubble footer in clicked by the user.
  void OnGooglePasswordManagerLinkClicked();

  // Called by the view code when the "Save it in Google Account" link in the
  // buuble footer is clicked by the user.
  void OnMovePasswordLinkClicked();

  // Returns the available credentials which match the current site.
  base::span<std::unique_ptr<password_manager::PasswordForm> const>
  GetCredentials() const;

  // Returns the credential for the single credential details mode. It must
  // be called when `bubble_mode()` returns `kSingleCredentialDetails` only.
  const password_manager::PasswordForm&
  GetSingleCredentialDetailsModeCredential() const;

  // Calls the password store backend to update the current details bubble
  // credential to `updated_form`.
  void UpdateDetailsBubbleCredentialInPasswordStore(
      password_manager::PasswordForm updated_form);

  // Calls OS-specific user authentication available via the
  // PasswordsModelDelegate. Upon successful reauth, the `password_form` is
  // the current details bubble credential, and `completion` is invoked
  void AuthenticateUserAndDisplayDetailsOf(
      password_manager::PasswordForm password_form,
      base::OnceCallback<void(bool)> completion);

  // Returns whether any of the available credentials matching the current site
  // has the same username value as `username`.
  bool UsernameExists(const std::u16string& username);

  // Returns whether user can currently use account storage.
  bool IsOptedInForAccountStorage() const;

  void set_details_bubble_credential(
      const std::optional<password_manager::PasswordForm>& password) {
    details_bubble_credential_ = password;
  }

  std::optional<password_manager::PasswordForm>
  get_details_bubble_credential() {
    return details_bubble_credential_;
  }

  BubbleMode bubble_mode() const { return bubble_mode_; }

 private:
  // Called when the favicon was retrieved. It invokes |favicon_ready_callback|
  // passing the retrieved favicon.
  void OnFaviconReady(
      base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback,
      const favicon_base::FaviconImageResult& result);

  // PasswordBubbleControllerBase methods:
  void ReportInteractions() override;

  // Returns the password store in which this password form is stored.
  scoped_refptr<password_manager::PasswordStoreInterface> PasswordStoreForForm(
      const password_manager::PasswordForm& password_form) const;

  // Is invoked upon completion of user reauth. If  `authentication_result` is
  // true, `password_form` becomes the current details bubble credential.
  // Invokes `completion` with the `authentication_result`.
  void OnUserAuthenticationCompleted(
      password_manager::PasswordForm password_form,
      base::OnceCallback<void(bool)> completion,
      bool authentication_result);

  const BubbleMode bubble_mode_;

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;

  // If not set, the bubble displays the list of all credentials stored for the
  // current domain. When set, the bubble displays the details of this
  // `PasswordForm`.
  std::optional<password_manager::PasswordForm> details_bubble_credential_;

  base::WeakPtrFactory<ManagePasswordsBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_
