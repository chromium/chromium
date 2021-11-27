// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

// A tab-modal dialog to allow a user signing in with a managed account
// to create a new Chrome profile.
class ProfileSigninConfirmationDialogViews : public views::DialogDelegateView {
 public:
  METADATA_HEADER(ProfileSigninConfirmationDialogViews);

  // Create and show the dialog, which owns itself.
  static void Show(
      Browser* browser,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
      bool prompt_for_new_profile);

  ProfileSigninConfirmationDialogViews(
      Browser* browser,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
      bool prompt_for_new_profile);
  ProfileSigninConfirmationDialogViews(
      const ProfileSigninConfirmationDialogViews&) = delete;
  ProfileSigninConfirmationDialogViews& operator=(
      const ProfileSigninConfirmationDialogViews&) = delete;
  ~ProfileSigninConfirmationDialogViews() override;

 private:
  // views::DialogDelegateView:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  void ContinueSigninButtonPressed();

  // Called when the "learn more" link is clicked.
  void LearnMoreClicked(const ui::Event& event);

  // Builds the default view for the dialog.
  void BuildDefaultView();

  // Build the view with the "work profile" wording enabled by
  // |features::SyncConfirmationUpdatedText|.
  void BuildWorkProfileView();

  // Weak ptr to parent view.
  const raw_ptr<Browser> browser_;

  // The GAIA username being signed in.
  std::string username_;

  // Dialog button handler.
  std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate_;

  // Whether the user should be prompted to create a new profile.
  const bool prompt_for_new_profile_;

  const bool use_work_profile_wording_;
};

BEGIN_VIEW_BUILDER(,
                   ProfileSigninConfirmationDialogViews,
                   views::DialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, ProfileSigninConfirmationDialogViews)

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
