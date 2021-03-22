// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/test_secure_payment_confirmation_payment_request_delegate.h"

#include "base/callback_helpers.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace payments {

TestSecurePaymentConfirmationPaymentRequestDelegate::
    TestSecurePaymentConfirmationPaymentRequestDelegate(
        content::RenderFrameHost* render_frame_host,
        base::WeakPtr<SecurePaymentConfirmationModel> model,
        SecurePaymentConfirmationDialogView::ObserverForTest* observer)
    : ChromePaymentRequestDelegate(render_frame_host),
      frame_routing_id_(content::GlobalFrameRoutingId(
          render_frame_host->GetProcess()->GetID(),
          render_frame_host->GetRoutingID())),
      model_(model),
      dialog_view_((new SecurePaymentConfirmationDialogView(
                        observer,
                        /*ui_observer_for_test=*/nullptr))
                       ->GetWeakPtr()) {}

TestSecurePaymentConfirmationPaymentRequestDelegate::
    ~TestSecurePaymentConfirmationPaymentRequestDelegate() = default;

void TestSecurePaymentConfirmationPaymentRequestDelegate::ShowDialog(
    base::WeakPtr<PaymentRequest> request) {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (rfh && rfh->IsCurrent()) {
    dialog_view_->ShowDialog(content::WebContents::FromRenderFrameHost(rfh),
                             model_->GetWeakPtr(), base::DoNothing(),
                             base::DoNothing());
  }
}

void TestSecurePaymentConfirmationPaymentRequestDelegate::CloseDialog() {
  dialog_view_->HideDialog();
}

}  // namespace payments
