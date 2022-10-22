// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

namespace content {
class WebContents;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

namespace payments {

// A delegate for presenting modal dialogs that are triggered from a web-based
// payment handler. Since the payment sheet is itself a modal dialog, the
// WebContentsModalDialogHost is expected to be borrowed from the browser that
// spawned the payment sheet. Observes the WebContents that spawned the payment
// handler.
class PaymentHandlerModalDialogManagerDelegate
    : public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  // |host| must not be null.
  explicit PaymentHandlerModalDialogManagerDelegate(
      content::WebContents* host_web_contents);

  PaymentHandlerModalDialogManagerDelegate(
      const PaymentHandlerModalDialogManagerDelegate&) = delete;
  PaymentHandlerModalDialogManagerDelegate& operator=(
      const PaymentHandlerModalDialogManagerDelegate&) = delete;

  ~PaymentHandlerModalDialogManagerDelegate() override;

  // Sets the |web_contents| that is behind the modal dialogs managed by this
  // modal dialog manager. |web_contents| must not be null.
  void SetWebContents(content::WebContents* web_contents);

  // WebContentsModalDialogManagerDelegate:
  // |web_contents| must not be null and is expected to be the same as the one
  // provided to SetWebContents().
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

 private:
  // The WebContents hosting the dialog.
  base::WeakPtr<content::WebContents> host_web_contents_;

  // A not-owned pointer to the WebContents behind the modal dialogs.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_MODAL_DIALOG_MANAGER_DELEGATE_H_
