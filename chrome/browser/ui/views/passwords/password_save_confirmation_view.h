// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_CONFIRMATION_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

// A view confirming to the user that a password was saved and offering a link
// to the Google account manager.
class PasswordSaveConfirmationView : public PasswordBubbleViewBase,
                                     public views::StyledLabelListener {
 public:
  explicit PasswordSaveConfirmationView(content::WebContents* web_contents,
                                        views::View* anchor_view,
                                        DisplayReason reason);
  ~PasswordSaveConfirmationView() override;

 private:
  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // LocationBarBubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  gfx::Size CalculatePreferredSize() const override;

  DISALLOW_COPY_AND_ASSIGN(PasswordSaveConfirmationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_CONFIRMATION_VIEW_H_
