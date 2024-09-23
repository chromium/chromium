// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_

#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace base {
class TimeDelta;
}

namespace autofill {

class CardUnmaskPromptController {
 public:
  // Interaction.
  virtual void OnUnmaskDialogClosed() = 0;
  virtual void OnUnmaskPromptAccepted(const std::u16string& cvc,
                                      const std::u16string& exp_month,
                                      const std::u16string& exp_year,
                                      bool enable_fido_auth,
                                      bool was_checkbox_visible) = 0;
  virtual void NewCardLinkClicked() = 0;

  // State.
#if BUILDFLAG(IS_IOS)
  // On IOS, a separate string other than the window title is needed to be shown
  // as the title of the navigation bar.
  virtual std::u16string GetNavigationTitle() const = 0;
#endif
  virtual std::u16string GetWindowTitle() const = 0;
  virtual std::u16string GetInstructionsMessage() const = 0;
  virtual std::u16string GetOkButtonLabel() const = 0;
  virtual int GetCvcImageRid() const = 0;
  virtual bool ShouldRequestExpirationDate() const = 0;
  // TODO(crbug.com/303715882): Should consider removing these detailed
  // information accessors and instead return the credit card object directly.
  // Only exposing necessary information is good but this list is growing
  // larger.
#if BUILDFLAG(IS_ANDROID)
  virtual Suggestion::Icon GetCardIcon() const = 0;
  virtual std::u16string GetCardName() const = 0;
  virtual std::u16string GetCardLastFourDigits() const = 0;
  virtual std::u16string GetCardExpiration() const = 0;
  virtual const GURL& GetCardArtUrl() const = 0;
  virtual int GetGooglePayImageRid() const = 0;
  virtual bool ShouldOfferWebauthn() const = 0;
  virtual bool GetWebauthnOfferStartState() const = 0;
  virtual std::u16string GetCvcImageAnnouncement() const = 0;
#endif
  virtual base::TimeDelta GetSuccessMessageDuration() const = 0;
  virtual payments::PaymentsAutofillClient::PaymentsRpcResult
  GetVerificationResult() const = 0;
  virtual bool IsVirtualCard() const = 0;
  virtual const CreditCard& GetCreditCard() const = 0;
#if !BUILDFLAG(IS_IOS)
  virtual int GetCvcTooltipResourceId() = 0;
#endif

  // Utilities.
  virtual bool InputCvcIsValid(const std::u16string& input_text) const = 0;
  virtual bool InputExpirationIsValid(const std::u16string& month,
                                      const std::u16string& year) const = 0;
  virtual int GetExpectedCvcLength() const = 0;
  virtual bool IsChallengeOptionPresent() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_
