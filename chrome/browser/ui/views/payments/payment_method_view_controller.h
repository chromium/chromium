// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_METHOD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_METHOD_VIEW_CONTROLLER_H_

#include "base/macros.h"
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
  PaymentMethodViewController(PaymentRequestSpec* spec,
                              PaymentRequestState* state,
                              PaymentRequestDialogView* dialog);
  ~PaymentMethodViewController() override;

 private:
  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  base::string16 GetSecondaryButtonLabel() override;
  int GetSecondaryButtonTag() override;
  int GetSecondaryButtonId() override;

  PaymentRequestItemList payment_method_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentMethodViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_METHOD_VIEW_CONTROLLER_H_
