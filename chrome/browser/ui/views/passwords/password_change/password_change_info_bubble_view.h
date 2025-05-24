// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

// Bubble view, which is displayed during the password change flow. It is shown
// when checking if user is signed into a web site.
class PasswordChangeInfoBubbleView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasswordChangeInfoBubbleView, PasswordBubbleViewBase)

 public:
  // Bubble body text label id. It's set here to be used in unit tests.
  static constexpr int kChangingPasswordBodyText = 1;

  PasswordChangeInfoBubbleView(content::WebContents* web_contents,
                               views::View* anchor_view,
                               PasswordChangeDelegate::State state);

 private:
  ~PasswordChangeInfoBubbleView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  std::unique_ptr<views::View> CreateBodyText(
      PasswordChangeDelegate::State state);

  PasswordChangeInfoBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_VIEW_H_
