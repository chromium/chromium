// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_NO_PASSWORD_CHANGE_FORM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_NO_PASSWORD_CHANGE_FORM_VIEW_H_

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

class NoPasswordChangeFormBubbleController;

// The dialog shown after no change password form has been found.
class NoPasswordChangeFormView : public PasswordBubbleViewBase {
  METADATA_HEADER(NoPasswordChangeFormView, PasswordBubbleViewBase)

 public:
  NoPasswordChangeFormView(content::WebContents* web_contents,
                           views::View* anchor_view);

 private:
  ~NoPasswordChangeFormView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // View:
  void OnWidgetInitialized() override;

  std::unique_ptr<NoPasswordChangeFormBubbleController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_NO_PASSWORD_CHANGE_FORM_VIEW_H_
