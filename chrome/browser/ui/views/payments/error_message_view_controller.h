// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_ERROR_MESSAGE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_ERROR_MESSAGE_VIEW_CONTROLLER_H_

#include <memory>

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

  ErrorMessageViewController(const ErrorMessageViewController&) = delete;
  ErrorMessageViewController& operator=(const ErrorMessageViewController&) =
      delete;

  ~ErrorMessageViewController() override;

 private:
  // PaymentRequestSheetController:
  std::u16string GetPrimaryButtonLabel() override;
  ButtonCallback GetPrimaryButtonCallback() override;
  int GetPrimaryButtonId() override;
  bool GetPrimaryButtonEnabled() override;
  bool ShouldShowHeaderBackArrow() override;
  bool ShouldShowSecondaryButton() override;
  std::u16string GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  bool GetSheetId(DialogViewID* sheet_id) override;
  bool ShouldAccelerateEnterKey() override;
  bool CanContentViewBeScrollable() override;
  base::WeakPtr<PaymentRequestSheetController> GetWeakPtr() override;

  // Must be the last member of a leaf class.
  base::WeakPtrFactory<ErrorMessageViewController> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_ERROR_MESSAGE_VIEW_CONTROLLER_H_
