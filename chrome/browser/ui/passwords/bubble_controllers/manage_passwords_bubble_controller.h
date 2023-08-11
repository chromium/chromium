// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/gfx/image/image.h"

class PasswordsModelDelegate;

namespace favicon_base {
struct FaviconImageResult;
}

namespace password_manager {
class PasswordStoreInterface;
enum class SyncState;
}  // namespace password_manager

// This controller provides data and actions for the ManagePasswordsView.
class ManagePasswordsBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit ManagePasswordsBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~ManagePasswordsBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;

  // Called by the view code when the manage button is clicked by the user.
  void OnManageClicked(password_manager::ManagePasswordsReferrer referrer);

  // Makes a request to the favicon service for the icon of current visible URL.
  // The request to the favicon store is canceled on destruction of the
  // controller.
  void RequestFavicon(
      base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback);

  password_manager::SyncState GetPasswordSyncState();

  // Returns the email of current primary account. Returns empty string if no
  // account is signed in.
  std::u16string GetPrimaryAccountEmail();

  // Called by the view code when the "Google Password Manager" link in the
  // bubble footer in clicked by the user.
  void OnGooglePasswordManagerLinkClicked();

  // Returns the available credentials which match the current site.
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  GetCredentials() const;

  // Calls the password store backend to update the currently selected password
  // to `updated_form`.
  void UpdateSelectedCredentialInPasswordStore(
      password_manager::PasswordForm updated_form);

  // Calls OS-specific user authentication available via the
  // PasswordsModelDelegate. Upon successful reauth, the `password_form` is the
  // currently selected credential, and `completion` is invoked
  void AuthenticateUserAndDisplayDetailsOf(
      password_manager::PasswordForm password_form,
      base::OnceCallback<void(bool)> completion);

  // Returns whether any of the available credentials matching the current site
  // has the same username value as `username`.
  bool UsernameExists(const std::u16string& username);

  void set_currently_selected_password(
      const absl::optional<password_manager::PasswordForm>& password) {
    currently_selected_password_ = password;
  }

  absl::optional<password_manager::PasswordForm>
  get_currently_selected_password() {
    return currently_selected_password_;
  }

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
  // true, `password_form` becomes the currently selected credential. Invokes
  // `completion` with the `authentication_result`.
  void OnUserAuthenticationCompleted(
      password_manager::PasswordForm password_form,
      base::OnceCallback<void(bool)> completion,
      bool authentication_result);

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;

  // If not set, the bubble displays the list of all credentials stored for the
  // current domain. When set, the bubble displays the password details of the
  // currently selected password.
  absl::optional<password_manager::PasswordForm> currently_selected_password_;

  base::WeakPtrFactory<ManagePasswordsBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MANAGE_PASSWORDS_BUBBLE_CONTROLLER_H_
