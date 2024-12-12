// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SIGN_IN_CHECK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SIGN_IN_CHECK_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

// Bubble view, which is displayed during the password change flow. It is shown
// when checking if user is signed into a web site.
class SignInCheckBubbleView : public PasswordBubbleViewBase {
  METADATA_HEADER(SignInCheckBubbleView, PasswordBubbleViewBase)

 public:
  SignInCheckBubbleView(content::WebContents* web_contents,
                        views::View* anchor_view);

 private:
  ~SignInCheckBubbleView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  PasswordChangeInfoBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_SIGN_IN_CHECK_BUBBLE_VIEW_H_
