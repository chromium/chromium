// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_AUTO_SIGN_IN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_AUTO_SIGN_IN_VIEW_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/passwords/bubble_controllers/auto_sign_in_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// A view containing just one credential that was used for for automatic signing
// in.
class PasswordAutoSignInView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasswordAutoSignInView, PasswordBubbleViewBase)

 public:
  PasswordAutoSignInView(content::WebContents* web_contents,
                         views::View* anchor_view);

  PasswordAutoSignInView(const PasswordAutoSignInView&) = delete;
  PasswordAutoSignInView& operator=(const PasswordAutoSignInView&) = delete;

#if defined(UNIT_TEST)
  static void set_auto_signin_toast_timeout(int seconds) {
    auto_signin_toast_timeout_ = seconds;
  }
#endif

 private:
  ~PasswordAutoSignInView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // LocationBarBubbleDelegateView:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  void OnTimer();
  static base::TimeDelta GetTimeout();

  AutoSignInBubbleController controller_;

  base::OneShotTimer timer_;

  // The timeout in seconds for the auto sign-in toast.
  static int auto_signin_toast_timeout_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_AUTO_SIGN_IN_VIEW_H_
