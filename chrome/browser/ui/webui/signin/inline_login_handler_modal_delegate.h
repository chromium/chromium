// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_MODAL_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_MODAL_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"

namespace chromeos {

// Used to display sub-modals inside |InlineLoginHandlerDialogChromeOS| modal
// dialog, e.g. displaying a dialog for accounts using 2FA with WebAuthn,
// where users can select alternate 2FAs.
class InlineLoginHandlerModalDelegate
    : public ChromeWebModalDialogManagerDelegate {
 public:
  // |host| is a non owning pointer to the host dialog of this delegate
  // (|InlineLoginHandlerDialogChromeOS|).
  explicit InlineLoginHandlerModalDelegate(
      web_modal::WebContentsModalDialogHost* host);
  ~InlineLoginHandlerModalDelegate() override;

  // web_modal::WebContentsModalDialogManagerDelegate overrides.
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

 private:
  // Non-owning pointer.
  web_modal::WebContentsModalDialogHost* host_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerModalDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_MODAL_DELEGATE_H_
