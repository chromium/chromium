// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_HANDLER_MODAL_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_HANDLER_MODAL_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"

namespace ash {

// Used to display sub-modals inside |InlineLoginHandlerDialog| modal
// dialog, e.g. displaying a dialog for accounts using 2FA with WebAuthn,
// where users can select alternate 2FAs.
class InlineLoginHandlerModalDelegate
    : public ChromeWebModalDialogManagerDelegate {
 public:
  // |host| is a non owning pointer to the host dialog of this delegate
  // (|InlineLoginHandlerDialog|).
  explicit InlineLoginHandlerModalDelegate(
      web_modal::WebContentsModalDialogHost* host);

  InlineLoginHandlerModalDelegate(const InlineLoginHandlerModalDelegate&) =
      delete;
  InlineLoginHandlerModalDelegate& operator=(
      const InlineLoginHandlerModalDelegate&) = delete;

  ~InlineLoginHandlerModalDelegate() override;

  // web_modal::WebContentsModalDialogManagerDelegate overrides.
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

 private:
  // Non-owning pointer.
  raw_ptr<web_modal::WebContentsModalDialogHost> host_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_HANDLER_MODAL_DELEGATE_H_
