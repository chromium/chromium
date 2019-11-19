// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_AUTO_SIGN_IN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_AUTO_SIGN_IN_VIEW_H_

#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// A view containing just one credential that was used for for automatic signing
// in.
class PasswordAutoSignInView : public PasswordBubbleViewBase,
                               public views::ButtonListener {
 public:
  PasswordAutoSignInView(content::WebContents* web_contents,
                         views::View* anchor_view,
                         DisplayReason reason);

#if defined(UNIT_TEST)
  static void set_auto_signin_toast_timeout(int seconds) {
    auto_signin_toast_timeout_ = seconds;
  }
#endif

 private:
  ~PasswordAutoSignInView() override;

  // LocationBarBubbleDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void OnTimer();
  static base::TimeDelta GetTimeout();

  base::OneShotTimer timer_;

  // The timeout in seconds for the auto sign-in toast.
  static int auto_signin_toast_timeout_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutoSignInView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_AUTO_SIGN_IN_VIEW_H_
