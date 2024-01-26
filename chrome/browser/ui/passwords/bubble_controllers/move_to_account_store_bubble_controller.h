// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_

#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/gfx/image/image.h"

class PasswordsModelDelegate;

namespace favicon_base {
struct FaviconImageResult;
}

// This controller manages the bubble asking the user to move a profile
// credential to their account store.
class MoveToAccountStoreBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit MoveToAccountStoreBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~MoveToAccountStoreBubbleController() override;

  // Called by the view when the user clicks the confirmation button.
  void AcceptMove();

  // Called by the view when the user clicks the "No, thanks" button.
  void RejectMove();

  // Returns either a an account avatar or a fallback icon of |size|.
  gfx::Image GetProfileIcon(int size);

  // Returns an email for current profile.
  std::u16string GetProfileEmail() const;

  // Makes a request to the favicon service for the icon of origin url against
  // which the passwords have been submitted.. The request to the favicon store
  // is canceled on destruction of the controller.
  void RequestFavicon(
      base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback);

 private:
  // Called when the favicon was retrieved. It invokes |favicon_ready_callback|
  // passing the retrieved favicon.
  void OnFaviconReady(
      base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback,
      const favicon_base::FaviconImageResult& result);

  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_MOVE_TO_ACCOUNT_STORE_BUBBLE_CONTROLLER_H_
