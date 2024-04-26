// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE_DIALOG_H_
#define CHROME_BROWSER_UI_PROFILES_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/profiles/profile_customization_synced_theme_waiter.h"
#include "chrome/browser/ui/signin/signin_modal_dialog.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "google_apis/gaia/core_account_id.h"

class Browser;

// First run experience modal dialog shown after the user created a new profile
// through the signin interception.
//
// First run consists of the following steps in order:
// - Sync confirmation, if Sync can be enabled for this account
// - Profile customization, if profile theme isn't overridden by a policy
// - Profile switching IPH (outside of the dialog, after it closes)
// If neither of the conditions is satisfied, the dialog never shows and
// silently deletes itself through calling `on_close_callback`.
class SigninInterceptFirstRunExperienceDialog
    : public SigninModalDialog,
      public SigninViewControllerDelegate::Observer {
 public:
  // Dialog steps and user actions that occur during the first run experience.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // TODO(crbug.com/40209493): Add further buckets to track engagement
  // with the profile customization dialog (customized name / customized color).
  enum class DialogEvent {
    // FRE started.
    kStart = 0,
    // Sync confirmation was shown to the user.
    kShowSyncConfirmation = 1,
    // The user turned sync on.
    kSyncConfirmationClickConfirm = 2,
    // The user canceled sync.
    kSyncConfirmationClickCancel = 3,
    // The user clicked on sync settings.
    kSyncConfirmationClickSettings = 4,
    // Profile customization was shown to the user.
    kShowProfileCustomization = 5,
    // The user completed profile customization.
    kProfileCustomizationClickDone = 6,
    // The user skipped profile customization.
    kProfileCustomizationClickSkip = 7,

    kMaxValue = kProfileCustomizationClickSkip
  };

  explicit SigninInterceptFirstRunExperienceDialog(
      Browser* browser,
      const CoreAccountId& account_id,
      bool is_forced_intercept,
      base::OnceClosure on_close_callback);
  ~SigninInterceptFirstRunExperienceDialog() override;

  // Shows the dialog. The dialog might decide to close synchronously which
  // shouldn't happen within a constructor.
  void Show();

  // SigninModalDialog:
  void CloseModalDialog() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetModalDialogWebContentsForTesting() override;

  // SigninViewControllerDelegate::Observer:
  void OnModalDialogClosed() override;

 private:
  // `InterceptTurnSyncOnHelperDelegate` needs access to private methods of
  // `SigninInterceptFirstRunExperienceDialog`.
  class InterceptTurnSyncOnHelperDelegate;
  friend class SigninInterceptFirstRunExperienceDialogBrowserTest;

  // Ordered list of first run steps. Some steps might be skipped but they
  // always appear in this order.
  enum class Step {
    kStart,
    kTurnOnSync,
    kSyncConfirmation,
    kWaitForSyncedTheme,
    kProfileCustomization,
    kProfileSwitchIPHAndCloseModal,
  };

  // Moves the dialog from `expected_current_step` to `step`.
  void DoNextStep(Step expected_current_step, Step step);

  // Actions executed right after moving to a corresponding step.
  void DoTurnOnSync();
  void DoSyncConfirmation();
  void DoWaitForSyncedTheme();
  void DoProfileCustomization();
  void DoProfileSwitchIPHAndCloseModal();

  void SetDialogDelegate(SigninViewControllerDelegate* delegate);
  void PreloadProfileCustomizationUI();
  void OnSyncedThemeReady(
      ProfileCustomizationSyncedThemeWaiter::Outcome outcome);
  void ProfileCustomizationCloseOnCompletion(
      ProfileCustomizationHandler::CustomizationResult customization_result);

  const raw_ptr<Browser> browser_;
  const CoreAccountId account_id_;
  const bool is_forced_intercept_;

  Step current_step_ = Step::kStart;

  raw_ptr<SigninViewControllerDelegate> dialog_delegate_ = nullptr;
  base::ScopedObservation<SigninViewControllerDelegate,
                          SigninViewControllerDelegate::Observer>
      dialog_delegate_observation_{this};

  std::unique_ptr<content::WebContents>
      profile_customization_preloaded_contents_;
  std::unique_ptr<ProfileCustomizationSyncedThemeWaiter> synced_theme_waiter_;

  base::WeakPtrFactory<SigninInterceptFirstRunExperienceDialog>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PROFILES_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE_DIALOG_H_
