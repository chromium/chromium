// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_WEBID_DIALOG_H_
#define CHROME_BROWSER_UI_WEBID_WEBID_DIALOG_H_

#include <string>

#include "base/callback.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

using UserApproval = content::IdentityRequestDialogController::UserApproval;
using PermissionCallback =
    content::IdentityRequestDialogController::InitialApprovalCallback;
using CloseCallback =
    content::IdentityRequestDialogController::IdProviderWindowClosedCallback;

// The interface for creating and controlling a platform-dependent WebIdDialog.
class WebIdDialog {
 public:
  static WebIdDialog* Create(content::WebContents* rp_web_contents);

  // Creates and shows a confirmation dialog for initial permission. The
  // provided callback is called with appropriate status depending on whether
  // user accepted or denied/closed the dialog.
  virtual void ShowInitialPermission(const std::u16string& idp_hostname,
                                     const std::u16string& rp_hostname,
                                     PermissionCallback) = 0;

  // Creates and shows a confirmation dialog for return permission. The provided
  // callback is called with appropriate status depending on whether user
  // accepted or denied/closed the dialog.
  virtual void ShowTokenExchangePermission(const std::u16string& idp_hostname,
                                           const std::u16string& rp_hostname,
                                           PermissionCallback) = 0;

  // Creates and shows a window that loads the identity provider sign in page at
  // the given URL. The provided callback is called when IDP has provided an
  // id_token with the id_token a its argument, or when window is closed by user
  // with an empty string as its argument.
  virtual void ShowSigninPage(content::WebContents* idp_web_contents,
                              const GURL& idp_signin_url,
                              CloseCallback) = 0;

  // Closes the sign in page. Calling the close callback that was provided
  // previously.
  virtual void CloseSigninPage() = 0;

  content::WebContents* rp_web_contents() const { return rp_web_contents_; }

 protected:
  explicit WebIdDialog(content::WebContents* rp_web_contents);
  WebIdDialog(const WebIdDialog&) = delete;
  WebIdDialog& operator=(const WebIdDialog&) = delete;
  virtual ~WebIdDialog() = default;

 private:
  content::WebContents* rp_web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBID_WEBID_DIALOG_H_
