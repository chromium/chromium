// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "ui/gfx/geometry/size.h"

namespace payments {

class PaymentRequestDialogViewTestApi {
 public:
  explicit PaymentRequestDialogViewTestApi(PaymentRequestDialogView* view)
      : view_(CHECK_DEREF(view)) {}
  PaymentRequestDialogViewTestApi(const PaymentRequestDialogViewTestApi&) =
      delete;
  PaymentRequestDialogViewTestApi& operator=(
      const PaymentRequestDialogViewTestApi&) = delete;
  ~PaymentRequestDialogViewTestApi() = default;

  static base::WeakPtr<PaymentRequestDialogView> CreateDialogView(
      base::WeakPtr<PaymentRequest> request,
      base::WeakPtr<PaymentRequestDialogView::ObserverForTest> observer);

  void ConfirmPayment() { view_->ConfirmPaymentForTesting(); }

  gfx::Size CalculatePreferredSize(const views::SizeBounds& available_size) {
    return view_->CalculatePreferredSize(available_size);
  }

  ViewStack* view_stack() { return view_->view_stack_; }
  ControllerMap* controller_map() { return &view_->controller_map_; }
  views::View* throbber_overlay() { return view_->throbber_overlay_; }
  PaymentAppLoadingView* loading_view_overlay() {
    return view_->loading_view_overlay_;
  }

 private:
  const raw_ref<PaymentRequestDialogView> view_;
};

inline PaymentRequestDialogViewTestApi test_api(
    PaymentRequestDialogView* view) {
  return PaymentRequestDialogViewTestApi(view);
}

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_TEST_API_H_
