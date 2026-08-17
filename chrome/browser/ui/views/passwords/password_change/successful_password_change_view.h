// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Label;
class ToggleImageButton;
}  // namespace views

class SuccessfulPasswordChangeBubbleController;

// Bubble view, which is displayed when password change flow is successfully
// finished.
class SuccessfulPasswordChangeView : public PasswordBubbleViewBase {
  METADATA_HEADER(SuccessfulPasswordChangeView, PasswordBubbleViewBase)

 public:
  // Bubble UI element ids. It's set here to be used in unit tests.
  static constexpr int kUsernameLabelId = 1;
  static constexpr int kPasswordLabelId = 2;
  static constexpr int kEyeIconButtonId = 3;
  static constexpr int kManagePasswordsButtonId = 4;

  SuccessfulPasswordChangeView(content::WebContents* web_contents,
                               views::BubbleAnchor anchor_view);

 private:
  ~SuccessfulPasswordChangeView() override;

  // PasswordBubbleViewBase:
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;
  ui::ImageModel GetWindowIcon() override;

  // views::View:
  void AddedToWidget() override;

  // Creates row containing favicon, username, password and eye icon:
  // *--------------------------------------------------------*
  // |         | Username                          |          |
  // | Favicon |-----------------------------------| Eye icon |
  // |         | Password                          |          |
  // *--------------------------------------------------------*
  std::unique_ptr<views::View> CreateUsernamePasswordWithEyeIcon();

  // Callback for handling the eye icon (password reveal button) click.
  void OnEyeIconClicked();
  // Callback for handling the result of the authentication (revealing password
  // may require authentication).
  void OnAuthenticationResult(bool auth_result);

  std::unique_ptr<SuccessfulPasswordChangeBubbleController> controller_;

  raw_ptr<views::Label> password_label_ = nullptr;
  raw_ptr<views::ToggleImageButton> eye_icon_ = nullptr;

  base::WeakPtrFactory<SuccessfulPasswordChangeView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_VIEW_H_
