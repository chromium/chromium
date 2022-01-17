// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE_DIALOG_H_
#define CHROME_BROWSER_UI_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE_DIALOG_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/signin_modal_dialog.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
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
  explicit SigninInterceptFirstRunExperienceDialog(
      Browser* browser,
      const CoreAccountId& account_id,
      base::OnceClosure on_close_callback);
  ~SigninInterceptFirstRunExperienceDialog() override;

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
    kProfileCustomization,
    kProfileSwitchIPHAndCloseModal,
  };

  // Moves the dialog from `expected_current_step` to `step`.
  void DoNextStep(Step expected_current_step, Step step);

  // Actions executed right after moving to a corresponding step.
  void DoTurnOnSync();
  void DoSyncConfirmation();
  void DoProfileCustomization();
  void DoProfileSwitchIPHAndCloseModal();

  void SetDialogDelegate(SigninViewControllerDelegate* delegate);
  void PreloadProfileCustomizationUI();
  void OnProfileCustomizationDoneButtonClicked();

  const raw_ptr<Browser> browser_;
  const CoreAccountId account_id_;

  Step current_step_ = Step::kStart;

  raw_ptr<SigninViewControllerDelegate> dialog_delegate_ = nullptr;
  base::ScopedObservation<SigninViewControllerDelegate,
                          SigninViewControllerDelegate::Observer>
      dialog_delegate_observation_{this};

  std::unique_ptr<content::WebContents>
      profile_customization_preloaded_contents_;

  base::WeakPtrFactory<SigninInterceptFirstRunExperienceDialog>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE_DIALOG_H_
