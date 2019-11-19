// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_CVC_UNMASK_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_CVC_UNMASK_VIEW_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace autofill {
class AutofillClient;
}

namespace content {
class WebContents;
}

namespace views {
class Textfield;
}

namespace payments {

class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

class CvcUnmaskViewController
    : public PaymentRequestSheetController,
      public autofill::RiskDataLoader,
      public autofill::payments::FullCardRequest::UIDelegate,
      public views::ComboboxListener,
      public views::TextfieldController {
 public:
  CvcUnmaskViewController(
      PaymentRequestSpec* spec,
      PaymentRequestState* state,
      PaymentRequestDialogView* dialog,
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate,
      content::WebContents* web_contents);
  ~CvcUnmaskViewController() override;

  // autofill::RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // autofill::payments::FullCardRequest::UIDelegate:
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      autofill::AutofillClient::UnmaskCardReason reason,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      autofill::AutofillClient::PaymentsRpcResult result) override;

 protected:
  // PaymentRequestSheetController:
  base::string16 GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  std::unique_ptr<views::Button> CreatePrimaryButton() override;
  bool ShouldShowSecondaryButton() override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  // Called when the user confirms their CVC. This will pass the value to the
  // active FullCardRequest.
  void CvcConfirmed();

  // Display a label with the text |error|
  void DisplayError(base::string16 error);

  // Updates the enabled state of the pay button
  void UpdatePayButtonState();

  bool GetSheetId(DialogViewID* sheet_id) override;
  views::View* GetFirstFocusedView() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  autofill::MonthComboboxModel month_combobox_model_;
  autofill::YearComboboxModel year_combobox_model_;
  views::Textfield* cvc_field_;  // owned by the view hierarchy, outlives this.
  autofill::CreditCard credit_card_;
  content::WebContents* web_contents_;
  autofill::payments::PaymentsClient payments_client_;
  autofill::payments::FullCardRequest full_card_request_;
  base::WeakPtr<autofill::CardUnmaskDelegate> unmask_delegate_;

  base::WeakPtrFactory<CvcUnmaskViewController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CvcUnmaskViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_CVC_UNMASK_VIEW_CONTROLLER_H_
