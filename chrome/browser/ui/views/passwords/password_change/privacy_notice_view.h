// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PRIVACY_NOTICE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PRIVACY_NOTICE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/privacy_notice_bubble_view_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

class PrivacyNoticeView : public PasswordBubbleViewBase {
  METADATA_HEADER(PrivacyNoticeView, PasswordBubbleViewBase)

 public:
  PrivacyNoticeView(content::WebContents* web_contents,
                    views::View* anchor_view);

 private:
  ~PrivacyNoticeView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  PrivacyNoticeBubbleViewController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PRIVACY_NOTICE_VIEW_H_
