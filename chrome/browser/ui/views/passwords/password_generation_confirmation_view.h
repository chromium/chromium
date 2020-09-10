// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_CONFIRMATION_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/generation_confirmation_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/view.h"

// A view confirming to the user that a generated password was saved and
// offering a link to the Google account manager.
class PasswordGenerationConfirmationView : public PasswordBubbleViewBase {
 public:
  explicit PasswordGenerationConfirmationView(
      content::WebContents* web_contents,
      views::View* anchor_view,
      DisplayReason reason);
  ~PasswordGenerationConfirmationView() override;

 private:
  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  void StyledLabelLinkClicked();

  // LocationBarBubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  GenerationConfirmationBubbleController controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationConfirmationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_GENERATION_CONFIRMATION_VIEW_H_
