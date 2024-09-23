// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_IBAN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_IBAN_BUBBLE_CONTROLLER_H_

#include <string>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;
class Iban;
enum class IbanBubbleType;

// Interface that exposes controller functionality to save and manage IBAN
// bubbles.
class IbanBubbleController {
 public:
  IbanBubbleController() = default;
  IbanBubbleController(const IbanBubbleController&) = delete;
  IbanBubbleController& operator=(const IbanBubbleController&) = delete;
  virtual ~IbanBubbleController() = default;

  // Returns a reference to the IbanBubbleController associated with the
  // given `web_contents`. If controller does not exist, this will create the
  // controller from the `web_contents` then return the reference.
  static IbanBubbleController* GetOrCreate(content::WebContents* web_contents);

  // Returns the title that should be displayed in the bubble.
  virtual std::u16string GetWindowTitle() const = 0;

  // Returns the explanatory text that should be displayed in the bubble.
  // Returns an empty string if no message should be displayed.
  virtual std::u16string GetExplanatoryMessage() const = 0;

  // Returns the button label text for IBAN save bubbles.
  virtual std::u16string GetAcceptButtonText() const = 0;
  virtual std::u16string GetDeclineButtonText() const = 0;

  // Returns the account info of the signed-in user.
  virtual AccountInfo GetAccountInfo() = 0;

  // Returns the IBAN that will be saved in save bubble view or the IBAN that
  // has been saved in manage bubble view.
  virtual const Iban& GetIban() const = 0;

  virtual base::OnceCallback<void(PaymentsBubbleClosedReason)>
  GetOnBubbleClosedCallback() = 0;

  virtual AutofillBubbleBase* GetPaymentBubbleView() const = 0;

  // Interaction.
  virtual void OnAcceptButton(const std::u16string& nickname) = 0;
  virtual void OnLegalMessageLinkClicked(const GURL& url) = 0;
  virtual void OnManageSavedIbanExtraButtonClicked() = 0;
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;

  // Returns empty vector if no legal message should be shown.
  virtual const LegalMessageLines& GetLegalMessageLines() const = 0;
  // Returns true iff the bubble for upload save is showing or has been shown.
  virtual bool IsUploadSave() const = 0;
  // Returns the current state of the bubble.
  virtual IbanBubbleType GetBubbleType() const = 0;

  // Returns the UI parameters needed to display the IBAN upload save
  // confirmation view.
  virtual const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
  GetConfirmationUiParams() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_IBAN_BUBBLE_CONTROLLER_H_
