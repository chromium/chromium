// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

class SuccessfulPasswordChangeBubbleController;

// Bubble view, which is displayed when password change flow is successfully
// finished.
class SuccessfulPasswordChangeView : public PasswordBubbleViewBase {
  METADATA_HEADER(SuccessfulPasswordChangeView, PasswordBubbleViewBase)

 public:
  // Bubble UI element ids. It's set here to be used in unit tests.
  static constexpr int kBodyTextLabelId = 1;
  static constexpr int kUsernameLabelId = 2;
  static constexpr int kPasswordLabelId = 3;
  static constexpr int kEyeIconButtonId = 4;
  static constexpr int kManagePasswordsButtonId = 5;

  SuccessfulPasswordChangeView(content::WebContents* web_contents,
                               views::View* anchor_view);

 private:
  ~SuccessfulPasswordChangeView() override;

  std::unique_ptr<views::View> CreateFooterView();

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // View:
  void AddedToWidget() override;

  std::unique_ptr<SuccessfulPasswordChangeBubbleController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_VIEW_H_
