// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SHARED_PASSWORDS_NOTIFICATIONS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SHARED_PASSWORDS_NOTIFICATIONS_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

namespace password_manager {
struct PasswordForm;
}

// This controller manages the bubble notifying the user that some of the stored
// passwords have been received via password sharing feature from other users.
class SharedPasswordsNotificationBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit SharedPasswordsNotificationBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~SharedPasswordsNotificationBubbleController() override;

  // Called by the view code when the "Got it" button is clicked
  // by the user acknowledging the shared password notification prompt.
  void OnAcknowledgeClicked();

  // Called by the view code when the "Manage Passwords" button is clicked
  // by the user.
  void OnManagePasswordsClicked();

  // Called by the view code when the user clicks the close bubble button.
  void OnCloseBubbleClicked();

  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

 private:
  // Returns a list of the credentials currently stored in the delegate that
  // have been received via password sharing feature, and the user has not been
  // notified about them yet.
  std::vector<password_manager::PasswordForm*>
  GetSharedCredentialsRequiringNotification() const;

  // Marks all credentials returned by
  // GetSharedCredentialsRequiringNotification() in the password store as
  // notified, in the sense that the user has been notified about them and no
  // need to show the notification again.
  void MarkSharedCredentialAsNotifiedInPasswordStore();
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SHARED_PASSWORDS_NOTIFICATIONS_BUBBLE_CONTROLLER_H_
