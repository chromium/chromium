// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "components/payments/content/payment_request.h"

namespace payments {

// static
base::WeakPtr<PaymentRequestDialogView>
PaymentRequestDialogViewTestApi::CreateDialogView(
    base::WeakPtr<PaymentRequest> request,
    base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer) {
  return (new PaymentRequestDialogView(request, observer))
      ->weak_ptr_factory_.GetWeakPtr();
}

content::WebContents*
PaymentRequestDialogViewTestApi::GetPaymentHandlerWebContents() {
  auto it = controller_map()->find(view_stack()->top());
  CHECK(it != controller_map()->end());
  return static_cast<PaymentHandlerWebFlowViewController*>(it->second.get())
      ->web_contents();
}

}  // namespace payments
