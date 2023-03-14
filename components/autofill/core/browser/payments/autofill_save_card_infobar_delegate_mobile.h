// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/image/image.h"

struct AccountInfo;
class MockAutofillSaveCardInfoBarDelegateMobile;

namespace autofill {

class CreditCard;

// An InfoBarDelegate that enables the user to allow or deny storing credit
// card information gathered from a form submission. Only used on mobile.
class AutofillSaveCardInfoBarDelegateMobile : public ConfirmInfoBarDelegate {
 public:
  static std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
  CreateForLocalSave(AutofillClient::SaveCreditCardOptions options,
                     const CreditCard& card,
                     AutofillClient::LocalSaveCardPromptCallback callback);

  static std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile>
  CreateForUploadSave(AutofillClient::SaveCreditCardOptions options,
                      const CreditCard& card,
                      AutofillClient::UploadSaveCardPromptCallback callback,
                      const LegalMessageLines& legal_message_lines,
                      const AccountInfo& displayed_target_account);

  AutofillSaveCardInfoBarDelegateMobile(
      const AutofillSaveCardInfoBarDelegateMobile&) = delete;
  AutofillSaveCardInfoBarDelegateMobile& operator=(
      const AutofillSaveCardInfoBarDelegateMobile&) = delete;

  ~AutofillSaveCardInfoBarDelegateMobile() override;

  // Returns |delegate| as an AutofillSaveCardInfoBarDelegateMobile, or nullptr
  // if it is of another type.
  static AutofillSaveCardInfoBarDelegateMobile* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  bool is_for_upload() const {
    return absl::holds_alternative<
        AutofillClient::UploadSaveCardPromptCallback>(callback_);
  }
  int issuer_icon_id() const { return issuer_icon_id_; }
  const std::u16string& card_label() const { return card_label_; }
  const std::u16string& card_sub_label() const { return card_sub_label_; }
  const LegalMessageLines& legal_message_lines() const {
    return legal_message_lines_;
  }
  const std::u16string& card_last_four_digits() const {
    return card_last_four_digits_;
  }
  const std::u16string& cardholder_name() const { return cardholder_name_; }
  const std::u16string& expiration_date_month() const {
    return expiration_date_month_;
  }
  const std::u16string& expiration_date_year() const {
    return expiration_date_year_;
  }
  const std::u16string& displayed_target_account_email() const {
    return displayed_target_account_email_;
  }
  const gfx::Image& displayed_target_account_avatar() const {
    return displayed_target_account_avatar_;
  }

  // Called when a link in the legal message text was clicked.
  virtual void OnLegalMessageLinkClicked(GURL url);

  // Google Pay branding is enabled with a flag and only for cards upstreamed
  // to Google.
  bool IsGooglePayBrandingEnabled() const;

  // Description text to be shown above the card information in the infobar.
  std::u16string GetDescriptionText() const;

  // ConfirmInfoBarDelegate:
  int GetIconId() const override;
  std::u16string GetMessageText() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

#if BUILDFLAG(IS_IOS)
  // Updates and then saves the card using |cardholder_name|,
  // |expiration_date_month| and |expiration_date_year|, which were provided
  // as part of the iOS save card Infobar dialog.
  virtual bool UpdateAndAccept(std::u16string cardholder_name,
                               std::u16string expiration_date_month,
                               std::u16string expiration_date_year);
#endif  // BUILDFLAG(IS_IOS)

 private:
  friend class ::MockAutofillSaveCardInfoBarDelegateMobile;

  // If an `callback` is an upload callback, `displayed_target_account` should
  // be the account to which the card will be saved.
  AutofillSaveCardInfoBarDelegateMobile(
      AutofillClient::SaveCreditCardOptions options,
      const CreditCard& card,
      absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                    AutofillClient::UploadSaveCardPromptCallback> callback,
      const LegalMessageLines& legal_message_lines,
      const AccountInfo& displayed_target_account);

  // Runs the appropriate local or upload save callback with the given
  // |user_decision|, using the |user_provided_details|. If
  // |user_provided_details| is empty then the current Card values will be used.
  // The  cardholder name and expiration date portions of
  // |user_provided_details| are handled separately, so if either of them are
  // empty the current Card values will be used.
  void RunSaveCardPromptCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      AutofillClient::UserProvidedCardDetails user_provided_details);

  void LogUserAction(AutofillMetrics::InfoBarMetric user_action);

  // If the cardholder name is missing, request the name from the user before
  // saving the card. If the expiration date is missing, request the missing
  // data from the user before saving the card.
  AutofillClient::SaveCreditCardOptions options_;

  // The callback to run once the user makes a decision with respect to the
  // credit card offer-to-save prompt.
  absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                AutofillClient::UploadSaveCardPromptCallback>
      callback_;

  // Did the user ever explicitly accept or dismiss this infobar?
  bool had_user_interaction_;

  // The resource ID for the icon that identifies the issuer of the card.
  int issuer_icon_id_;

  // The label for the card to show in the content of the infobar.
  std::u16string card_label_;

  // The sub-label for the card to show in the content of the infobar.
  std::u16string card_sub_label_;

  // The last four digits of the card for which save is being offered.
  std::u16string card_last_four_digits_;

  // The card holder name of the card for which save is being offered.
  std::u16string cardholder_name_;

  // The expiration month of the card for which save is being offered.
  std::u16string expiration_date_month_;

  // The expiration year of the card for which save is being offered.
  std::u16string expiration_date_year_;

  // The legal message lines to show in the content of the infobar.
  LegalMessageLines legal_message_lines_;

  // Information the infobar should display about the account where the card
  // will be saved. Both the email and avatar can be empty, e.g. if the card
  // won't be saved to any account (just locally) or the target account
  // shouldn't appear.
  std::u16string displayed_target_account_email_;
  gfx::Image displayed_target_account_avatar_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_DELEGATE_MOBILE_H_
