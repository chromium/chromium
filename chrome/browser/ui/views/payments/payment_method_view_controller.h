// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_METHOD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_METHOD_VIEW_CONTROLLER_H_

#include "chrome/browser/ui/views/payments/payment_request_item_list.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

// The PaymentRequestSheetController subtype for the Payment Method screen of
// the Payment Request flow.
class PaymentMethodViewController : public PaymentRequestSheetController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  PaymentMethodViewController(base::WeakPtr<PaymentRequestSpec> spec,
                              base::WeakPtr<PaymentRequestState> state,
                              base::WeakPtr<PaymentRequestDialogView> dialog);

  PaymentMethodViewController(const PaymentMethodViewController&) = delete;
  PaymentMethodViewController& operator=(const PaymentMethodViewController&) =
      delete;

  ~PaymentMethodViewController() override;

 private:
  // PaymentRequestSheetController:
  std::u16string GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  bool ShouldShowPrimaryButton() override;
  bool ShouldShowSecondaryButton() override;
  std::u16string GetSecondaryButtonLabel() override;
  int GetSecondaryButtonId() override;
  base::WeakPtr<PaymentRequestSheetController> GetWeakPtr() override;

  PaymentRequestItemList payment_method_list_;

  // Must be the last member of a leaf class.
  base::WeakPtrFactory<PaymentMethodViewController> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_METHOD_VIEW_CONTROLLER_H_
