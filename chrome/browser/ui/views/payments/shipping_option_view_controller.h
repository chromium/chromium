// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_OPTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_OPTION_VIEW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_item_list.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/payment_request_spec.h"

namespace payments {

class PaymentRequestState;

class ShippingOptionViewController : public PaymentRequestSheetController,
                                     public PaymentRequestSpec::Observer {
 public:
  ShippingOptionViewController(base::WeakPtr<PaymentRequestSpec> spec,
                               base::WeakPtr<PaymentRequestState> state,
                               base::WeakPtr<PaymentRequestDialogView> dialog);

  ShippingOptionViewController(const ShippingOptionViewController&) = delete;
  ShippingOptionViewController& operator=(const ShippingOptionViewController&) =
      delete;

  ~ShippingOptionViewController() override;

  // PaymentRequestSpec::Observer:
  void OnSpecUpdated() override;

 private:
  // PaymentRequestSheetController:
  std::u16string GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  std::unique_ptr<views::View> CreateExtraFooterView() override;
  bool ShouldShowPrimaryButton() override;
  bool ShouldShowSecondaryButton() override;
  base::WeakPtr<PaymentRequestSheetController> GetWeakPtr() override;

  PaymentRequestItemList shipping_option_list_;

  // Must be the last member of a leaf class.
  base::WeakPtrFactory<ShippingOptionViewController> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_OPTION_VIEW_CONTROLLER_H_
