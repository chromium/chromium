// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class Profile;

namespace autofill {

class CreditCard;
enum class BubbleType;

// Interface that exposes controller functionality to save card bubbles.
class SaveCardBubbleController {
 public:
  SaveCardBubbleController() = default;
  SaveCardBubbleController(const SaveCardBubbleController&) = delete;
  SaveCardBubbleController& operator=(const SaveCardBubbleController&) = delete;
  virtual ~SaveCardBubbleController() = default;

  // Returns a reference to the SaveCardBubbleController associated with the
  // given |web_contents|. If controller does not exist, this will create the
  // controller from the |web_contents| then return the reference.
  static SaveCardBubbleController* GetOrCreate(
      content::WebContents* web_contents);

  // Returns a reference to the SaveCardBubbleController associated with the
  // given |web_contents|. If controller does not exist, this will return
  // nullptr.
  static SaveCardBubbleController* Get(content::WebContents* web_contents);

  // Returns the title that should be displayed in the bubble.
  virtual std::u16string GetWindowTitle() const = 0;

  // Returns the explanatory text that should be displayed in the bubble.
  // Returns an empty string if no message should be displayed.
  virtual std::u16string GetExplanatoryMessage() const = 0;

  // Returns the button label text for save card bubbles.
  virtual std::u16string GetAcceptButtonText() const = 0;
  virtual std::u16string GetDeclineButtonText() const = 0;

  // Returns the account info of the signed-in user.
  virtual AccountInfo GetAccountInfo() = 0;

  // Returns the profile.
  virtual Profile* GetProfile() const = 0;

  // Returns the card that will be uploaded if the user accepts.
  virtual const CreditCard& GetCard() const = 0;

  // Returns whether the dialog should include a textfield requesting the user
  // to confirm/provide cardholder name.
  virtual bool ShouldRequestNameFromUser() const = 0;

  // Returns whether the dialog should include a pair of dropdown lists
  // allowing the user to provide expiration date.
  virtual bool ShouldRequestExpirationDateFromUser() const = 0;

  // Interaction.
  // OnSaveButton takes in a struct representing the cardholder name,
  // expiration date month and expiration date year confirmed/entered by the
  // user if they were requested, or struct with empty strings otherwise.
  virtual void OnSaveButton(const AutofillClient::UserProvidedCardDetails&
                                user_provided_card_details) = 0;
  virtual void OnLegalMessageLinkClicked(const GURL& url) = 0;
  virtual void OnManageCardsClicked() = 0;
  virtual void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) = 0;

  // State.

  // Returns empty vector if no legal message should be shown.
  virtual const LegalMessageLines& GetLegalMessageLines() const = 0;
  // Returns true iff is showing or has showed bubble for upload save.
  virtual bool IsUploadSave() const = 0;
  // Returns the current state of the bubble.
  virtual BubbleType GetBubbleType() const = 0;
  // Returns true if the user is signed in and sync transport is active for
  // Wallet data, without having turned on sync-the-feature.
  virtual bool IsPaymentsSyncTransportEnabledWithoutSyncFeature() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_H_
