// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"

namespace autofill {

class CardUnmaskPromptView;

// This class is owned by `ChromeAutofillClient`.
class CardUnmaskPromptControllerImpl : public CardUnmaskPromptController {
 public:
  explicit CardUnmaskPromptControllerImpl(PrefService* pref_service);

  CardUnmaskPromptControllerImpl(const CardUnmaskPromptControllerImpl&) =
      delete;
  CardUnmaskPromptControllerImpl& operator=(
      const CardUnmaskPromptControllerImpl&) = delete;

  virtual ~CardUnmaskPromptControllerImpl();

  // This should be OnceCallback<unique_ptr<CardUnmaskPromptView>> but there are
  // tests which don't do the ownership correctly.
  using CardUnmaskPromptViewFactory =
      base::OnceCallback<CardUnmaskPromptView*()>;

  // Functions called by ChromeAutofillClient.
  // It is guaranteed that |view_factory| is called before this function
  // returns, i.e., the callback will not outlive the stack frame of ShowPrompt.
  virtual void ShowPrompt(
      CardUnmaskPromptViewFactory view_factory,
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate);
  // The CVC the user entered went through validation.
  void OnVerificationResult(AutofillClient::PaymentsRpcResult result);

  // CardUnmaskPromptController implementation.
  void OnUnmaskDialogClosed() override;
  void OnUnmaskPromptAccepted(const std::u16string& cvc,
                              const std::u16string& exp_month,
                              const std::u16string& exp_year,
                              bool enable_fido_auth) override;
  void NewCardLinkClicked() override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetInstructionsMessage() const override;
  std::u16string GetOkButtonLabel() const override;
  int GetCvcImageRid() const override;
  bool ShouldRequestExpirationDate() const override;
#if BUILDFLAG(IS_ANDROID)
  std::string GetCardIconString() const override;
  std::u16string GetCardName() const override;
  std::u16string GetCardLastFourDigits() const override;
  std::u16string GetCardExpiration() const override;
  int GetGooglePayImageRid() const override;
  bool ShouldOfferWebauthn() const override;
  bool GetWebauthnOfferStartState() const override;
#endif
  bool InputCvcIsValid(const std::u16string& input_text) const override;
  bool InputExpirationIsValid(const std::u16string& month,
                              const std::u16string& year) const override;
  int GetExpectedCvcLength() const override;
  bool IsChallengeOptionPresent() const override;
  base::TimeDelta GetSuccessMessageDuration() const override;
  AutofillClient::PaymentsRpcResult GetVerificationResult() const override;
  bool IsVirtualCard() const override;
#if !BUILDFLAG(IS_IOS)
  int GetCvcTooltipResourceId() override;
#endif

 protected:
  // Exposed for testing.
  CardUnmaskPromptView* view() { return card_unmask_view_; }

 private:
  bool AllowsRetry(AutofillClient::PaymentsRpcResult result);
  bool IsCvcInFront();
  bool ShouldDismissUnmaskPromptUponResult(
      AutofillClient::PaymentsRpcResult result);
  void LogOnCloseEvents();
  AutofillMetrics::UnmaskPromptEvent GetCloseReasonEvent();

  CardUnmaskPromptOptions card_unmask_prompt_options_;
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
  bool new_card_link_clicked_ = false;
  CreditCard card_;
  base::WeakPtr<CardUnmaskDelegate> delegate_;
  raw_ptr<CardUnmaskPromptView> card_unmask_view_ = nullptr;

  AutofillClient::PaymentsRpcResult unmasking_result_ =
      AutofillClient::PaymentsRpcResult::kNone;
  int unmasking_number_of_attempts_ = 0;
  base::Time shown_timestamp_;
  // Timestamp of the last time the user clicked the Verify button.
  base::Time verify_timestamp_;

  CardUnmaskDelegate::UserProvidedUnmaskDetails pending_details_;

  base::WeakPtrFactory<CardUnmaskPromptControllerImpl> weak_pointer_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_
