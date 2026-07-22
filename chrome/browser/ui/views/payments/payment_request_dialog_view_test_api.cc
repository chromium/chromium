// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
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

}  // namespace payments
