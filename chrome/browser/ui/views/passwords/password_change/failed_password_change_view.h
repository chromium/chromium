// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_FAILED_PASSWORD_CHANGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_FAILED_PASSWORD_CHANGE_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

class FailedPasswordChangeBubbleController;

class FailedPasswordChangeView : public PasswordBubbleViewBase {
  METADATA_HEADER(FailedPasswordChangeView, PasswordBubbleViewBase)

 public:
  FailedPasswordChangeView(content::WebContents* web_contents,
                           views::View* anchor_view);

 private:
  ~FailedPasswordChangeView() override;

  std::unique_ptr<views::View> CreateFooterView();

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // View:
  void AddedToWidget() override;
  void OnWidgetInitialized() override;

  std::unique_ptr<FailedPasswordChangeBubbleController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_FAILED_PASSWORD_CHANGE_VIEW_H_
