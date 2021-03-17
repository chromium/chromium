// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UPDATE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UPDATE_BUBBLE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_ui.h"

class PasswordsModelDelegate;
namespace base {
class Clock;
}

// This controller provides data and actions for the PasswordSaveUpdateView.
class SaveUpdateBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit SaveUpdateBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      DisplayReason display_reason);
  ~SaveUpdateBubbleController() override;

  // Called by the view code when the save/update button is clicked by the user.
  void OnSaveClicked();

  // Called by the view code when the "Nope" button in clicked by the user in
  // update bubble.
  void OnNopeUpdateClicked();

  // Called by the view code when the "Never for this site." button in clicked
  // by the user.
  void OnNeverForThisSiteClicked();

  // Called by the view code when username or password is corrected using
  // the username correction or password selection features in PendingView.
  void OnCredentialEdited(std::u16string new_username,
                          std::u16string new_password);

  // The password bubble can switch its state between "save" and "update"
  // depending on the user input. |state_| only captures the correct state on
  // creation. This method returns true iff the current state is "update".
  bool IsCurrentStateUpdate() const;

  // Returns true iff the bubble is supposed to show the footer about syncing
  // to Google account.
  bool ShouldShowFooter() const;

  // Returns true and updates the internal state iff the Save bubble should
  // switch to show a promotion after the password was saved. Otherwise,
  // returns false and leaves the current state.
  bool ReplaceToShowPromotionIfNeeded();

  // Returns true if passwords revealing is not locked or re-authentication is
  // not available on the given platform. Otherwise, the method schedules
  // re-authentication and bubble reopen (the current bubble will be destroyed),
  // and returns false immediately. New bubble will reveal the passwords if the
  // re-authentication is successful.
  bool RevealPasswords();

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;

  password_manager::ui::State state() const { return state_; }

  const password_manager::PasswordForm& pending_password() const {
    return pending_password_;
  }

  bool are_passwords_revealed_when_bubble_is_opened() const {
    return are_passwords_revealed_when_bubble_is_opened_;
  }

  bool enable_editing() const { return enable_editing_; }

#if defined(UNIT_TEST)
  void set_clock(base::Clock* clock) { clock_ = clock; }

  void allow_passwords_revealing() {
    password_revealing_requires_reauth_ = false;
  }

  bool password_revealing_requires_reauth() const {
    return password_revealing_requires_reauth_;
  }
#endif

 private:
  // PasswordBubbleControllerBase methods:
  void ReportInteractions() override;

  // Origin of the page from where this bubble was triggered.
  url::Origin origin_;
  password_manager::ui::State state_;
  password_manager::PasswordForm pending_password_;
  std::vector<password_manager::PasswordForm> local_credentials_;
  password_manager::InteractionsStats interaction_stats_;
  password_manager::metrics_util::UIDisplayDisposition display_disposition_;

  // True iff password revealing should require re-auth for privacy reasons.
  bool password_revealing_requires_reauth_;

  // True iff username/password editing should be enabled.
  bool enable_editing_;

  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;

  // Used to retrieve the current time, in base::Time units.
  base::Clock* clock_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UPDATE_BUBBLE_CONTROLLER_H_
