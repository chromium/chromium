// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIAL_LEAK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIAL_LEAK_BUBBLE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "ui/views/controls/styled_label.h"

class PasswordChangeCredentialLeakBubbleView : public PasswordBubbleViewBase {
  METADATA_HEADER(PasswordChangeCredentialLeakBubbleView,
                  PasswordBubbleViewBase)

 public:
  PasswordChangeCredentialLeakBubbleView(content::WebContents* web_contents,
                                         views::View* anchor_view);

 private:
  ~PasswordChangeCredentialLeakBubbleView() override;

  std::unique_ptr<views::StyledLabel> CreateBodyText();

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // View:
  void OnWidgetInitialized() override;

  PasswordChangeCredentialLeakBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIAL_LEAK_BUBBLE_VIEW_H_
