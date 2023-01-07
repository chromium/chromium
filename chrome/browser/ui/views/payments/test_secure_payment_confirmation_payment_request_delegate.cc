// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_secure_payment_confirmation_payment_request_delegate.h"

#include "base/functional/callback_helpers.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace payments {

TestSecurePaymentConfirmationPaymentRequestDelegate::
    TestSecurePaymentConfirmationPaymentRequestDelegate(
        content::RenderFrameHost* render_frame_host,
        base::WeakPtr<SecurePaymentConfirmationModel> model,
        base::WeakPtr<SecurePaymentConfirmationDialogView::ObserverForTest>
            observer)
    : ChromePaymentRequestDelegate(render_frame_host),
      model_(model),
      dialog_view_((new SecurePaymentConfirmationDialogView(
                        observer,
                        /*ui_observer_for_test=*/nullptr))
                       ->GetWeakPtr()) {}

TestSecurePaymentConfirmationPaymentRequestDelegate::
    ~TestSecurePaymentConfirmationPaymentRequestDelegate() = default;

void TestSecurePaymentConfirmationPaymentRequestDelegate::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  if (rfh && rfh->IsActive()) {
    dialog_view_->ShowDialog(content::WebContents::FromRenderFrameHost(rfh),
                             model_->GetWeakPtr(), base::DoNothing(),
                             base::DoNothing(), base::DoNothing());
  }
}

void TestSecurePaymentConfirmationPaymentRequestDelegate::CloseDialog() {
  dialog_view_->HideDialog();
}

}  // namespace payments
