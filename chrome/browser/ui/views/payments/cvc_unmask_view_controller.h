// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_CVC_UNMASK_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_CVC_UNMASK_VIEW_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/risk_data_loader.h"
#include "content/public/browser/global_routing_id.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace autofill {
class AutofillClient;
}

namespace views {
class Combobox;
class Textfield;
}

namespace payments {

class PaymentRequestCvcUnmaskViewControllerVisualTest;
class PaymentRequestSpec;
class PaymentRequestState;
class PaymentRequestDialogView;

class CvcUnmaskViewController
    : public PaymentRequestSheetController,
      public autofill::RiskDataLoader,
      public autofill::payments::FullCardRequest::UIDelegate,
      public views::TextfieldController {
 public:
  CvcUnmaskViewController(
      base::WeakPtr<PaymentRequestSpec> spec,
      base::WeakPtr<PaymentRequestState> state,
      base::WeakPtr<PaymentRequestDialogView> dialog,
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate,
      content::RenderFrameHost* render_frame_host);

  CvcUnmaskViewController(const CvcUnmaskViewController&) = delete;
  CvcUnmaskViewController& operator=(const CvcUnmaskViewController&) = delete;

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
  std::u16string GetSheetTitle() override;
  void FillContentView(views::View* content_view) override;
  std::u16string GetPrimaryButtonLabel() override;
  ButtonCallback GetPrimaryButtonCallback() override;
  int GetPrimaryButtonId() override;
  bool GetPrimaryButtonEnabled() override;
  bool ShouldShowSecondaryButton() override;

 private:
  friend PaymentRequestCvcUnmaskViewControllerVisualTest;
  // Called when the user confirms their CVC. This will pass the value to the
  // active FullCardRequest.
  void CvcConfirmed();

  // Display a label with the text |error|
  void DisplayError(std::u16string error);

  // Updates the enabled state of the pay button
  void UpdatePayButtonState();

  bool GetSheetId(DialogViewID* sheet_id) override;
  views::View* GetFirstFocusedView() override;

  // PaymentRequestSheetController:
  void BackButtonPressed() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  void OnPerformAction();

  autofill::MonthComboboxModel month_combobox_model_;
  autofill::YearComboboxModel year_combobox_model_;
  raw_ptr<views::Combobox> month_combobox_ = nullptr;
  raw_ptr<views::Combobox> year_combobox_ = nullptr;
  raw_ptr<views::Textfield>
      cvc_field_;  // owned by the view hierarchy, outlives this.
  autofill::CreditCard credit_card_;
  const content::GlobalRenderFrameHostId frame_routing_id_;
  autofill::payments::PaymentsClient payments_client_;
  autofill::payments::FullCardRequest full_card_request_;
  base::WeakPtr<autofill::CardUnmaskDelegate> unmask_delegate_;

  base::WeakPtrFactory<CvcUnmaskViewController> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_CVC_UNMASK_VIEW_CONTROLLER_H_
