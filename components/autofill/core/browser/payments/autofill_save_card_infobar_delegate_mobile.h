// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class PrefService;

namespace autofill {

class CreditCard;

// An InfoBarDelegate that enables the user to allow or deny storing credit
// card information gathered from a form submission. Only used on mobile.
class AutofillSaveCardInfoBarDelegateMobile : public ConfirmInfoBarDelegate {
 public:
  AutofillSaveCardInfoBarDelegateMobile(
      bool upload,
      AutofillClient::SaveCreditCardOptions options,
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      AutofillClient::UploadSaveCardPromptCallback
          upload_save_card_prompt_callback,
      AutofillClient::LocalSaveCardPromptCallback
          local_save_card_prompt_callback,
      PrefService* pref_service,
      bool is_off_the_record);

  ~AutofillSaveCardInfoBarDelegateMobile() override;

  bool upload() const { return upload_; }
  int issuer_icon_id() const { return issuer_icon_id_; }
  const base::string16& card_label() const { return card_label_; }
  const base::string16& card_sub_label() const { return card_sub_label_; }
  const LegalMessageLines& legal_message_lines() const {
    return legal_message_lines_;
  }
  const base::string16& card_last_four_digits() const {
    return card_last_four_digits_;
  }
  const base::string16& cardholder_name() const { return cardholder_name_; }
  const base::string16& expiration_date_month() const {
    return expiration_date_month_;
  }
  const base::string16& expiration_date_year() const {
    return expiration_date_year_;
  }

  // Called when a link in the legal message text was clicked.
  void OnLegalMessageLinkClicked(GURL url);

  // Google Pay branding is enabled with a flag and only for cards upstreamed
  // to Google.
  bool IsGooglePayBrandingEnabled() const;

  // Description text to be shown above the card information in the infobar.
  base::string16 GetDescriptionText() const;

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  // Runs the appropriate local or upload save callback with the given
  // |user_decision|.
  void RunSaveCardPromptCallbackWithUserDecision(
      AutofillClient::SaveCardOfferUserDecision user_decision);

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // Whether the action is an upload or a local save.
  bool upload_;

  // If the cardholder name is missing, request the name from the user before
  // saving the card. If the expiration date is missing, request the missing
  // data from the user before saving the card.
  AutofillClient::SaveCreditCardOptions options_;

  // The callback to run once the user makes a decision with respect to the
  // credit card upload offer-to-save prompt (if |upload_| is true).
  AutofillClient::UploadSaveCardPromptCallback
      upload_save_card_prompt_callback_;

  // The callback to run once the user makes a decision with respect to the
  // local credit card offer-to-save prompt (if |upload_| is false).
  AutofillClient::LocalSaveCardPromptCallback local_save_card_prompt_callback_;

  // Weak reference to read & write |kAutofillAcceptSaveCreditCardPromptState|,
  PrefService* pref_service_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  // The resource ID for the icon that identifies the issuer of the card.
  int issuer_icon_id_;

  // The label for the card to show in the content of the infobar.
  base::string16 card_label_;

  // The sub-label for the card to show in the content of the infobar.
  base::string16 card_sub_label_;

  // The last four digits of the card for which save is being offered.
  base::string16 card_last_four_digits_;

  // The card holder name of the card for which save is being offered.
  base::string16 cardholder_name_;

  // The expiration month of the card for which save is being offered.
  base::string16 expiration_date_month_;

  // The expiration year of the card for which save is being offered.
  base::string16 expiration_date_year_;

  // The legal message lines to show in the content of the infobar.
  const LegalMessageLines& legal_message_lines_;

  // Whether the save is offered while off the record
  bool is_off_the_record_;

  DISALLOW_COPY_AND_ASSIGN(AutofillSaveCardInfoBarDelegateMobile);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
