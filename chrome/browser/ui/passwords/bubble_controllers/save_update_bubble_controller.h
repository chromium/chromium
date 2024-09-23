// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UPDATE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UPDATE_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/common_saved_account_manager_bubble_controller.h"

namespace base {
class Clock;
}

// This controller provides data and actions for the PasswordSaveUpdateView.
class SaveUpdateBubbleController
    : public CommonSavedAccountManagerBubbleController {
 public:
  explicit SaveUpdateBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      DisplayReason display_reason);
  ~SaveUpdateBubbleController() override;

  // Called by the view code when the save/update button is clicked by the user.
  void OnSaveClicked();

  // Called by the view code when the "Never for this site." button in clicked
  // by the user.
  void OnNeverForThisSiteClicked();

  // The password bubble can switch its state between "save" and "update"
  // depending on the user input. |state_| only captures the correct state on
  // creation. This method returns true iff the current state is "update".
  bool IsCurrentStateUpdate() const;

  // Returns true iff the bubble is supposed to show the footer about syncing
  // to Google account.
  bool ShouldShowFooter() const;

  // This method returns true iff the current state is "save" or "update" to a
  // password that is synced to the Google Account. This method covers
  // non-syncing account-store users as well as syncing users.
  bool IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount();

  // Invokes `callback` with true if passwords revealing is not locked or
  // re-authentication is not available on the given platform. Otherwise, the
  // method schedules re-authentication and invokes `callback` with the result
  // // of authentication.
  void ShouldRevealPasswords(
      PasswordsModelDelegate::AvailabilityCallback callback);

  // Returns true iff the password account store is used.
  bool IsUsingAccountStore();

  // Returns true if the user must opt-in to the account-scoped password storage
  // before the save bubble action can be concluded.
  bool IsAccountStorageOptInRequiredBeforeSave();

  // Users need to reauth to their account to opt-in using their password
  // account storage. This method returns whether account auth attempt during
  // the last password save process failed or not.
  bool DidAuthForAccountStoreOptInFail() const;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;

#if defined(UNIT_TEST)
  void set_clock(base::Clock* clock) { clock_ = clock; }

  bool password_revealing_requires_reauth() const {
    return password_revealing_requires_reauth_;
  }
#endif

 private:
  void ReportInteractions() override;

  password_manager::InteractionsStats interaction_stats_;

  // True iff password revealing should require re-auth for privacy reasons.
  bool password_revealing_requires_reauth_;

  // Used to retrieve the current time, in base::Time units.
  raw_ptr<base::Clock> clock_;

  std::vector<password_manager::PasswordForm> existing_credentials_;

  std::u16string original_username_;

  base::WeakPtrFactory<SaveUpdateBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_SAVE_UPDATE_BUBBLE_CONTROLLER_H_
