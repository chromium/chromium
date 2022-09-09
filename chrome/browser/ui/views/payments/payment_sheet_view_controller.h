// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_SHEET_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_SHEET_VIEW_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"

namespace payments {

class PaymentRequestDialogView;
class PaymentRequestRowView;

// The PaymentRequestSheetController subtype for the Payment Sheet screen of the
// Payment Request dialog.
class PaymentSheetViewController : public PaymentRequestSheetController,
                                   public PaymentRequestSpec::Observer,
                                   public PaymentRequestState::Observer {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  // The `spec` and `state` objects should not be null.
  PaymentSheetViewController(base::WeakPtr<PaymentRequestSpec> spec,
                             base::WeakPtr<PaymentRequestState> state,
                             base::WeakPtr<PaymentRequestDialogView> dialog);

  PaymentSheetViewController(const PaymentSheetViewController&) = delete;
  PaymentSheetViewController& operator=(const PaymentSheetViewController&) =
      delete;

  ~PaymentSheetViewController() override;

  // PaymentRequestSpec::Observer:
  void OnSpecUpdated() override;

  // PaymentRequestState::Observer:
  void OnGetAllPaymentAppsFinished() override {}
  void OnSelectedInformationChanged() override;

  void ButtonPressed(base::RepeatingClosure closure);

 private:
  // PaymentRequestSheetController:
  std::u16string GetSecondaryButtonLabel() override;
  bool ShouldShowHeaderBackArrow() override;
  std::u16string GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  std::unique_ptr<views::View> CreateExtraFooterView() override;

  // These functions create the various sections and rows of the payment sheet.
  // Where applicable, they also populate |accessible_content|, which shouldn't
  // be null, with the screen reader string that represents their contents.
  std::unique_ptr<views::View> CreateShippingSectionContent(
      std::u16string* accessible_content);
  std::unique_ptr<PaymentRequestRowView> CreateShippingRow();
  std::unique_ptr<PaymentRequestRowView> CreatePaymentSheetSummaryRow();
  std::unique_ptr<PaymentRequestRowView> CreatePaymentMethodRow();
  std::unique_ptr<views::View> CreateContactInfoSectionContent(
      std::u16string* accessible_content);
  std::unique_ptr<PaymentRequestRowView> CreateContactInfoRow();
  std::unique_ptr<PaymentRequestRowView> CreateShippingOptionRow();
  std::unique_ptr<views::View> CreateDataSourceRow();

  void AddShippingButtonPressed();
  void AddContactInfoButtonPressed();
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_SHEET_VIEW_CONTROLLER_H_
