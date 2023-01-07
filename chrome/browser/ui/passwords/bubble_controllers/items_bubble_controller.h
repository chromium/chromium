// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ITEMS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ITEMS_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "ui/gfx/image/image.h"

class PasswordsModelDelegate;

namespace favicon_base {
struct FaviconImageResult;
}

namespace password_manager {
struct PasswordForm;
enum class SyncState;
}  // namespace password_manager

// This controller provides data and actions for the PasswordItemsView.
class ItemsBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit ItemsBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~ItemsBubbleController() override;

  // Called by the view code when the manage button is clicked by the user.
  void OnManageClicked(password_manager::ManagePasswordsReferrer referrer);

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnPasswordAction(const password_manager::PasswordForm& password_form,
                        PasswordAction action);

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
  const std::vector<password_manager::PasswordForm>& local_credentials() const {
    return local_credentials_;
  }

 private:
  // Called when the favicon was retrieved. It invokes |favicon_ready_callback|
  // passing the retrieved favicon.
  void OnFaviconReady(
      base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback,
      const favicon_base::FaviconImageResult& result);

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  const std::vector<password_manager::PasswordForm> local_credentials_;

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  const std::u16string title_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_ITEMS_BUBBLE_CONTROLLER_H_
