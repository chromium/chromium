// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller.h"

namespace autofill {

class CardExpirationDateFixFlowView;

// Enables the user to accept or deny expiration date fix flow prompt.
// Only used on mobile. This class is responsible for its destruction.
// Destruction is achieved by calling delete when the prompt is
// dismissed.
class CardExpirationDateFixFlowControllerImpl
    : public CardExpirationDateFixFlowController {
 public:
  CardExpirationDateFixFlowControllerImpl();
  ~CardExpirationDateFixFlowControllerImpl() override;

  void Show(CardExpirationDateFixFlowView* card_expiration_date_fix_flow_view,
            const CreditCard& card,
            base::OnceCallback<void(const base::string16&,
                                    const base::string16&)> callback);

  // CardExpirationDateFixFlowController implementation.
  void OnAccepted(const base::string16& month,
                  const base::string16& year) override;
  void OnDismissed() override;
  void OnDialogClosed() override;
  int GetIconId() const override;
  base::string16 GetTitleText() const override;
  base::string16 GetSaveButtonLabel() const override;
  base::string16 GetCardLabel() const override;
  base::string16 GetCancelButtonLabel() const override;
  base::string16 GetInputLabel() const override;
  base::string16 GetDateSeparator() const override;
  base::string16 GetInvalidDateError() const override;

 private:
  // View that displays the fix flow prompt.
  CardExpirationDateFixFlowView* card_expiration_date_fix_flow_view_ = nullptr;

  // The callback to save the credit card to Google Payments once user accepts
  // fix flow.
  base::OnceCallback<void(const base::string16&, const base::string16&)>
      upload_save_card_callback_;

  // Whether the prompt was shown to the user.
  bool shown_ = false;

  // Did the user ever explicitly accept or dismiss this prompt?
  bool had_user_interaction_ = false;

  // Label of the card describing the network and the last four digits.
  base::string16 card_label_;

  DISALLOW_COPY_AND_ASSIGN(CardExpirationDateFixFlowControllerImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_IMPL_H_
