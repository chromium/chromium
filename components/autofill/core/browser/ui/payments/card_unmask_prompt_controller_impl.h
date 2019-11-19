// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"

namespace autofill {

class CardUnmaskPromptView;

class CardUnmaskPromptControllerImpl : public CardUnmaskPromptController {
 public:
  CardUnmaskPromptControllerImpl(PrefService* pref_service,
                                 bool is_off_the_record);
  virtual ~CardUnmaskPromptControllerImpl();

  // This should be OnceCallback<unique_ptr<CardUnmaskPromptView>> but there are
  // tests which don't do the ownership correctly.
  using CardUnmaskPromptViewFactory =
      base::OnceCallback<CardUnmaskPromptView*()>;

  // Functions called by ChromeAutofillClient.
  // It is guaranteed that |view_factory| is called before this function
  // returns, i.e., the callback will not outlive the stack frame of ShowPrompt.
  void ShowPrompt(CardUnmaskPromptViewFactory view_factory,
                  const CreditCard& card,
                  AutofillClient::UnmaskCardReason reason,
                  base::WeakPtr<CardUnmaskDelegate> delegate);
  // The CVC the user entered went through validation.
  void OnVerificationResult(AutofillClient::PaymentsRpcResult result);

  // CardUnmaskPromptController implementation.
  void OnUnmaskDialogClosed() override;
  void OnUnmaskPromptAccepted(const base::string16& cvc,
                              const base::string16& exp_month,
                              const base::string16& exp_year,
                              bool should_store_pan,
                              bool enable_fido_auth) override;
  void NewCardLinkClicked() override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetInstructionsMessage() const override;
  base::string16 GetOkButtonLabel() const override;
  int GetCvcImageRid() const override;
  bool ShouldRequestExpirationDate() const override;
  bool CanStoreLocally() const override;
  bool GetStoreLocallyStartState() const override;
  bool GetWebauthnOfferStartState() const override;
  bool InputCvcIsValid(const base::string16& input_text) const override;
  bool InputExpirationIsValid(const base::string16& month,
                              const base::string16& year) const override;
  int GetExpectedCvcLength() const override;
  base::TimeDelta GetSuccessMessageDuration() const override;
  AutofillClient::PaymentsRpcResult GetVerificationResult() const override;

 protected:
  // Exposed for testing.
  CardUnmaskPromptView* view() { return card_unmask_view_; }
  void SetCreditCardForTesting(CreditCard test_card) { card_ = test_card; }

 private:
  bool AllowsRetry(AutofillClient::PaymentsRpcResult result);
  void LogOnCloseEvents();
  AutofillMetrics::UnmaskPromptEvent GetCloseReasonEvent();

  PrefService* pref_service_;
  bool new_card_link_clicked_ = false;
  bool is_off_the_record_;
  CreditCard card_;
  AutofillClient::UnmaskCardReason reason_;
  base::WeakPtr<CardUnmaskDelegate> delegate_;
  CardUnmaskPromptView* card_unmask_view_ = nullptr;

  AutofillClient::PaymentsRpcResult unmasking_result_ = AutofillClient::NONE;
  bool unmasking_initial_should_store_pan_ = false;
  int unmasking_number_of_attempts_ = 0;
  base::Time shown_timestamp_;
  // Timestamp of the last time the user clicked the Verify button.
  base::Time verify_timestamp_;

  CardUnmaskDelegate::UserProvidedUnmaskDetails pending_details_;

  base::WeakPtrFactory<CardUnmaskPromptControllerImpl> weak_pointer_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptControllerImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_
