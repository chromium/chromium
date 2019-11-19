// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_OPTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_OPTION_VIEW_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/payment_request_item_list.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/payment_request_spec.h"

namespace payments {

class PaymentRequestState;

class ShippingOptionViewController : public PaymentRequestSheetController,
                                     public PaymentRequestSpec::Observer {
 public:
  ShippingOptionViewController(PaymentRequestSpec* spec,
                               PaymentRequestState* state,
                               PaymentRequestDialogView* dialog);
  ~ShippingOptionViewController() override;

  // PaymentRequestSpec::Observer:
  void OnSpecUpdated() override;

 private:
  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  std::unique_ptr<views::View> CreateExtraFooterView() override;
  bool ShouldShowSecondaryButton() override;

  PaymentRequestItemList shipping_option_list_;

  DISALLOW_COPY_AND_ASSIGN(ShippingOptionViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_OPTION_VIEW_CONTROLLER_H_
