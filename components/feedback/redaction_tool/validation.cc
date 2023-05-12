// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a copy of //components/autofill/core/browser/validation.cc ~2023. It
// should be used only by //components/feedback/redaction_tool/.
// We need a copy because the //components/feedback/redaction_tool source code
// is shared into ChromeOS and needs to have no dependencies outside of //base/.
// TODO(b/281812289) Deduplicate the logic and let the autofill component
// depend on this one.

#include "components/feedback/redaction_tool/validation.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace {
enum class CreditCardIssuer {
  kAmericanExpressCard,
  kDinersCard,
  kDiscoverCard,
  kGenericCard,
  kJCBCard,
  kMasterCard,
  kMirCard,
  kTroyCard,
  kUnionPay,
  kVisaCard,
  // Elo card is missing here as it's gated behind a feature flag. Once the card
  // is GA this file should be updated here as well.
};

CreditCardIssuer GetCardNetwork(const std::string& number) {
  // Credit card number specifications taken from:
  // https://en.wikipedia.org/wiki/Payment_card_number,
  // http://www.regular-expressions.info/creditcard.html,
  // https://developer.ean.com/general-info/valid-card-types,
  // http://www.bincodes.com/, and
  // http://www.fraudpractice.com/FL-binCC.html.
  // (Last updated: March 2021; change Troy bin range)
  //
  // Card Type              Prefix(es)                                  Length
  // --------------------------------------------------------------------------
  // Visa                   4                                          13,16,19
  // American Express       34,37                                      15
  // Diners Club            300-305,309,36,38-39                       14
  // Discover Card          6011,644-649,65                            16
  // JCB                    3528-3589                                  16
  // Mastercard             2221-2720, 51-55                           16
  // MIR                    2200-2204                                  16
  // Troy                   22050-22052, 9792                          16
  // UnionPay               62                                         16-19

  // Check for prefixes of length 6.
  if (number.size() >= 6) {
    int first_six_digits = 0;
    if (!base::StringToInt(number.substr(0, 6), &first_six_digits)) {
      return CreditCardIssuer::kGenericCard;
    }
  }

  // Check for prefixes of length 5.
  if (number.size() >= 5) {
    int first_five_digits = 0;
    if (!base::StringToInt(number.substr(0, 5), &first_five_digits)) {
      return CreditCardIssuer::kGenericCard;
    }

    if (first_five_digits == 22050 || first_five_digits == 22051 ||
        first_five_digits == 22052) {
      return CreditCardIssuer::kTroyCard;
    }
  }

  // Check for prefixes of length 4.
  if (number.size() >= 4) {
    int first_four_digits = 0;
    if (!base::StringToInt(number.substr(0, 4), &first_four_digits)) {
      return CreditCardIssuer::kGenericCard;
    }

    if (first_four_digits >= 2200 && first_four_digits <= 2204) {
      return CreditCardIssuer::kMirCard;
    }

    if (first_four_digits == 9792) {
      return CreditCardIssuer::kTroyCard;
    }

    if (first_four_digits >= 2221 && first_four_digits <= 2720) {
      return CreditCardIssuer::kMasterCard;
    }

    if (first_four_digits >= 3528 && first_four_digits <= 3589) {
      return CreditCardIssuer::kJCBCard;
    }

    if (first_four_digits == 6011) {
      return CreditCardIssuer::kDiscoverCard;
    }
  }

  // Check for prefixes of length 3.
  if (number.size() >= 3) {
    int first_three_digits = 0;
    if (!base::StringToInt(number.substr(0, 3), &first_three_digits)) {
      return CreditCardIssuer::kGenericCard;
    }

    if ((first_three_digits >= 300 && first_three_digits <= 305) ||
        first_three_digits == 309) {
      return CreditCardIssuer::kDinersCard;
    }

    if (first_three_digits >= 644 && first_three_digits <= 649) {
      return CreditCardIssuer::kDiscoverCard;
    }
  }

  // Check for prefixes of length 2.
  if (number.size() >= 2) {
    int first_two_digits = 0;
    if (!base::StringToInt(number.substr(0, 2), &first_two_digits)) {
      return CreditCardIssuer::kGenericCard;
    }

    if (first_two_digits == 34 || first_two_digits == 37) {
      return CreditCardIssuer::kAmericanExpressCard;
    }

    if (first_two_digits == 36 || first_two_digits == 38 ||
        first_two_digits == 39) {
      return CreditCardIssuer::kDinersCard;
    }

    if (first_two_digits >= 51 && first_two_digits <= 55) {
      return CreditCardIssuer::kMasterCard;
    }

    if (first_two_digits == 62) {
      return CreditCardIssuer::kUnionPay;
    }

    if (first_two_digits == 65) {
      return CreditCardIssuer::kDiscoverCard;
    }
  }

  // Check for prefixes of length 1.
  if (number.empty()) {
    return CreditCardIssuer::kGenericCard;
  }

  if (number[0] == '4') {
    return CreditCardIssuer::kVisaCard;
  }

  return CreditCardIssuer::kGenericCard;
}

bool HasCorrectLength(const std::string& number) {
  using enum CreditCardIssuer;
  // Credit card numbers are at most 19 digits in length, 12 digits seems to
  // be a fairly safe lower-bound [1].  Specific card issuers have more rigidly
  // defined sizes.
  // (Last updated: May 29, 2017)
  // [1] https://en.wikipedia.org/wiki/Payment_card_number.
  if (number.size() < 12 || number.size() > 19) {
    return false;
  }
  const std::vector<char> kUnlikelyIin{'0', '7', '8', '9'};
  if (base::Contains(kUnlikelyIin, number.front())) {
    return false;
  }

  const CreditCardIssuer type = GetCardNetwork(number);
  if (type == kGenericCard) {
    return true;
  }

  switch (number.size()) {
    case 13:
      return type == kVisaCard;
    case 14:
      return type == kDinersCard;
    case 15:
      return type == kAmericanExpressCard;
    case 16:
      return (type == kDiscoverCard || type == kJCBCard ||
              type == kMasterCard || type == kMirCard || type == kTroyCard ||
              type == kUnionPay || type == kVisaCard);
    case 17:
      [[fallthrough]];
    case 18:
      return type == kUnionPay;
    case 19:
      return (type == kUnionPay || type == kVisaCard);
    default: {
      return false;
    }
  }
}

bool PassesLuhnCheck(const std::string& number) {
  // Use the Luhn formula [3] to validate the number.
  // [3] http://en.wikipedia.org/wiki/Luhn_algorithm
  int sum = 0;
  bool odd = false;
  for (const char c : base::Reversed(number)) {
    if (!base::IsAsciiDigit(c)) {
      return false;
    }

    int digit = c - '0';
    if (odd) {
      digit *= 2;
      sum += digit / 10 + digit % 10;
    } else {
      sum += digit;
    }
    odd = !odd;
  }

  return (sum % 10) == 0;
}

}  // namespace

namespace redaction {

bool IsValidCreditCardNumber(const std::string& number) {
  if (!HasCorrectLength(number)) {
    return false;
  }

  return PassesLuhnCheck(number);
}

}  // namespace redaction
