// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class Profile;

// A tab-modal dialog to allow a user signing in with a managed account
// to create a new Chrome profile.
class ProfileSigninConfirmationDialogViews : public views::DialogDelegateView,
                                             public views::StyledLabelListener,
                                             public views::ButtonListener {
 public:
  // Create and show the dialog, which owns itself.
  static void ShowDialog(
      Browser* browser,
      Profile* profile,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate);

  ProfileSigninConfirmationDialogViews(
      Browser* browser,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
      bool prompt_for_new_profile);
  ~ProfileSigninConfirmationDialogViews() override;

 private:
  static void Show(
      Browser* browser,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
      bool prompt_for_new_profile);

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;
  ui::ModalType GetModalType() const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // views::WidgetDelegate::
  void WindowClosing() override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button*, const ui::Event& event) override;

  // Weak ptr to parent view.
  Browser* const browser_;

  // The GAIA username being signed in.
  std::string username_;

  // Dialog button handler.
  std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate_;

  // Whether the user should be prompted to create a new profile.
  const bool prompt_for_new_profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
