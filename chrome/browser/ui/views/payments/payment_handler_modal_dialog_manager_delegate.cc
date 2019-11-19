// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_modal_dialog_manager_delegate.h"

#include "chrome/browser/platform_util.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentHandlerModalDialogManagerDelegate::
    PaymentHandlerModalDialogManagerDelegate(
        web_modal::WebContentsModalDialogHost* host)
    : host_(host), web_contents_(nullptr) {
  DCHECK(host);
}

void PaymentHandlerModalDialogManagerDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  DCHECK(web_contents);
  DCHECK_EQ(web_contents_, web_contents);
  if (!blocked) {
    web_contents->Focus();
  }
}

web_modal::WebContentsModalDialogHost*
PaymentHandlerModalDialogManagerDelegate::GetWebContentsModalDialogHost() {
  return host_;
}

bool PaymentHandlerModalDialogManagerDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  DCHECK_EQ(web_contents_, web_contents);
  return platform_util::IsVisible(web_contents->GetNativeView());
}

void PaymentHandlerModalDialogManagerDelegate::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  web_contents_ = web_contents;
}

}  // namespace payments
