// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_AUTOFILL_CARD_VALIDATION_H_
#define COMPONENTS_PAYMENTS_CORE_AUTOFILL_CARD_VALIDATION_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"

namespace autofill {

class CreditCard;
class AutofillProfile;

}  // namespace autofill

namespace payments {

// Used to express the completion status of a credit card. Bit field values are
// identical to CompletionStatus fields in AutofillPaymentInstrument.java.
// Please modify AutofillPaymentInstrument.java after changing these bits since
// missing fields on both Android and Desktop are recorded in the same UMA
// metric: PaymentRequest.MissingPaymentFields.
typedef uint32_t CreditCardCompletionStatus;
static const CreditCardCompletionStatus CREDIT_CARD_COMPLETE = 0;
static const CreditCardCompletionStatus CREDIT_CARD_EXPIRED = 1 << 0;
static const CreditCardCompletionStatus CREDIT_CARD_NO_CARDHOLDER = 1 << 1;
static const CreditCardCompletionStatus CREDIT_CARD_NO_NUMBER = 1 << 2;
static const CreditCardCompletionStatus CREDIT_CARD_NO_BILLING_ADDRESS = 1 << 3;
static const CreditCardCompletionStatus CREDIT_CARD_TYPE_MISMATCH = 1 << 4;

// Returns the credit card's completion status. If equal to
// CREDIT_CARD_COMPLETE, then the card is ready to be used for Payment Request.
CreditCardCompletionStatus GetCompletionStatusForCard(
    const autofill::CreditCard& credit_card,
    const std::string& app_locale,
    const std::vector<autofill::AutofillProfile*> billing_addresses);

// Returns the credit card's completeness score. The score is used for sorting
// available cards before showing them to the user in payment sheet. Different
// fields are weighted according to the effort needed to complete them. The
// weights are set so that the number of missing fields matters most (i.e. cards
// with any three missing fields are scored lower than cards with any two
// missing fields which are in turn scored lower than cards with only one
// missing field). When number of missing fields are equal the order of
// importance is 1-missing card number 2-missing address 3-missing card holder's
// name 4-invalid expiry date. A complete card gets the highest score which is
// 37 and each score represents a unique set of missing/invalid fields (i.e. Two
// cards will tie if and only if they both have identical missing/invalid
// fields).
uint32_t GetCompletenessScore(
    const autofill::CreditCard& credit_card,
    const std::string& app_locale,
    const std::vector<autofill::AutofillProfile*> billing_addresses);

// Return the message to be displayed to the user, indicating what's missing
// to make the credit card complete for payment. If more than one thing is
// missing, the message will be a generic "more information required".
base::string16 GetCompletionMessageForCard(CreditCardCompletionStatus status);

// Returns the title string for a card edit dialog. The title string will
// mention what needs to be added/fixed to make the card valid if it is not
// valid. Otherwise, it will be "Edit card".
base::string16 GetEditDialogTitleForCard(CreditCardCompletionStatus status);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_AUTOFILL_CARD_VALIDATION_H_
