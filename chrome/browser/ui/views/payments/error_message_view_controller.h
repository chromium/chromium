// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_ERROR_MESSAGE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_ERROR_MESSAGE_VIEW_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

namespace views {
class View;
}

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

// The PaymentRequestSheetController subtype for the Error Message screen of the
// Payment Request flow.
class ErrorMessageViewController : public PaymentRequestSheetController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  ErrorMessageViewController(base::WeakPtr<PaymentRequestSpec> spec,
                             base::WeakPtr<PaymentRequestState> state,
                             base::WeakPtr<PaymentRequestDialogView> dialog);
  ~ErrorMessageViewController() override;

 private:
  // PaymentRequestSheetController:
  std::unique_ptr<views::Button> CreatePrimaryButton() override;
  bool ShouldShowHeaderBackArrow() override;
  bool ShouldShowSecondaryButton() override;
  base::string16 GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;

  DISALLOW_COPY_AND_ASSIGN(ErrorMessageViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_ERROR_MESSAGE_VIEW_CONTROLLER_H_
