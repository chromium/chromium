// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/autofill_card_validation.h"

#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

CreditCardCompletionStatus GetCompletionStatusForCard(
    const autofill::CreditCard& card,
    const std::string& app_locale,
    const std::vector<autofill::AutofillProfile*> billing_addresses) {
  CreditCardCompletionStatus status = CREDIT_CARD_COMPLETE;
  if (card.IsExpired(autofill::AutofillClock::Now()))
    status |= CREDIT_CARD_EXPIRED;

  if (card.number().empty())
    status |= CREDIT_CARD_NO_NUMBER;

  if (card.GetInfo(autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL),
                   app_locale)
          .empty()) {
    status |= CREDIT_CARD_NO_CARDHOLDER;
  }

  autofill::AutofillProfile* billing_address = nullptr;
  if (card.billing_address_id().empty() ||
      !(billing_address =
            autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
                card.billing_address_id(), billing_addresses))) {
    status |= CREDIT_CARD_NO_BILLING_ADDRESS;
  } else if (!autofill::addressinput::HasAllRequiredFields(
                 *autofill::i18n::CreateAddressDataFromAutofillProfile(
                     *billing_address, app_locale))) {
    // Incomplete billing addresses are treated as missing since the user
    // intraction for editing/completing the address is still required.
    status |= CREDIT_CARD_NO_BILLING_ADDRESS;
  }

  return status;
}

std::u16string GetCompletionMessageForCard(CreditCardCompletionStatus status) {
  switch (status) {
    // No message is shown for complete or expired card (which will be fixable)
    // in the CVC screen.
    case CREDIT_CARD_COMPLETE:
    case CREDIT_CARD_EXPIRED:
      return std::u16string();
    case CREDIT_CARD_NO_CARDHOLDER:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_NAME_ON_CARD_REQUIRED);
    case CREDIT_CARD_NO_NUMBER:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE);
    case CREDIT_CARD_NO_BILLING_ADDRESS:
      return l10n_util::GetStringUTF16(
          IDS_PAYMENTS_CARD_BILLING_ADDRESS_REQUIRED);
    default:
      // Multiple things are missing
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_MORE_INFORMATION_REQUIRED);
  }
}

std::u16string GetEditDialogTitleForCard(CreditCardCompletionStatus status) {
  switch (status) {
    case CREDIT_CARD_COMPLETE:
    case CREDIT_CARD_EXPIRED:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_EDIT_CARD);
    case CREDIT_CARD_NO_CARDHOLDER:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_NAME_ON_CARD);
    case CREDIT_CARD_NO_NUMBER:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_VALID_CARD_NUMBER);
    case CREDIT_CARD_NO_BILLING_ADDRESS:
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_BILLING_ADDRESS);
    default:
      // Multiple things are missing
      return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_MORE_INFORMATION);
  }
}

}  // namespace payments
