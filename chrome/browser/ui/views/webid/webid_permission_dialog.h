// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_DIALOG_H_

#include "base/callback.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}  // namespace content

// Basic permission dialog that is used to ask for user approval in various
// WebID flows.
//
// It shows a message with two buttons to cancel/accept e.g.,
// "Would you like to do X?  [Cancel]  [Continue]
class WebIdPermissionDialog : public views::BubbleDialogDelegateView {
 public:
  using UserApproval = content::IdentityRequestDialogController::UserApproval;
  using Callback =
      content::IdentityRequestDialogController::InitialApprovalCallback;

  WebIdPermissionDialog(content::WebContents* rp_web_contents,
                        std::unique_ptr<views::View> content,
                        Callback callback);
  ~WebIdPermissionDialog() override;
  void Show();

 private:
  bool Accept() override;
  bool Cancel() override;

  content::WebContents* rp_web_contents_;
  Callback callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_PERMISSION_DIALOG_H_
