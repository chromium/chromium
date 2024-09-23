// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/credit_card_number_validation.h"

#include <stddef.h>

#include <string>
#include <string_view>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"

namespace autofill {

bool IsValidCreditCardNumber(std::u16string_view text) {
  const std::u16string number = StripCardNumberSeparators(text);
  return HasCorrectCreditCardNumberLength(number) && PassesLuhnCheck(number);
}

bool HasCorrectCreditCardNumberLength(std::u16string_view number) {
  // Credit card numbers are at most 19 digits in length, 12 digits seems to
  // be a fairly safe lower-bound [1].  Specific card issuers have more rigidly
  // defined sizes.
  // (Last updated: May 29, 2017)
  // [1] https://en.wikipedia.org/wiki/Payment_card_number.
  // CardEditor.isCardNumberLengthMaxium() needs to be kept in sync.
  const char* const type = GetCardNetwork(number);
  if (type == kAmericanExpressCard && number.size() != 15)
    return false;
  if (type == kDinersCard && number.size() != 14)
    return false;
  if (type == kDiscoverCard && number.size() != 16)
    return false;
  if (type == kEloCard && number.size() != 16)
    return false;
  if (type == kJCBCard && number.size() != 16)
    return false;
  if (type == kMasterCard && number.size() != 16)
    return false;
  if (type == kMirCard && number.size() != 16)
    return false;
  if (type == kTroyCard && number.size() != 16)
    return false;
  if (type == kUnionPay && (number.size() < 16 || number.size() > 19))
    return false;
  if (type == kVerveCard && number.size() != 16 && number.size() != 18 &&
      number.size() != 19)
    return false;
  if (type == kVisaCard && number.size() != 13 && number.size() != 16 &&
      number.size() != 19)
    return false;
  if (type == kGenericCard && (number.size() < 12 || number.size() > 19))
    return false;

  return true;
}

bool PassesLuhnCheck(std::u16string_view number) {
  // Use the Luhn formula [3] to validate the number.
  // [3] http://en.wikipedia.org/wiki/Luhn_algorithm
  int sum = 0;
  bool odd = false;
  for (char16_t c : base::Reversed(number)) {
    if (!base::IsAsciiDigit(c))
      return false;

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

std::u16string StripCardNumberSeparators(std::u16string_view number) {
  std::u16string stripped;
  base::RemoveChars(number, u"- ", &stripped);
  return stripped;
}

const char* GetCardNetwork(std::u16string_view number) {
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
  // Elo                    See Elo regex pattern below                16
  // JCB                    3528-3589                                  16
  // Mastercard             2221-2720, 51-55                           16
  // MIR                    2200-2204                                  16
  // Troy                   22050-22052, 9792                          16
  // UnionPay               62                                         16-19
  // Verve                  506099–506198,507865-507964,650002–650027  16,18,19

  // Determine the network for the given |number| by going from the longest
  // (most specific) prefix to the shortest (most general) prefix.
  std::u16string stripped_number = StripCardNumberSeparators(number);

  // Original Elo parsing included only 6 BIN prefixes. This regex pattern,
  // sourced from the official Elo documentation, attempts to cover missing gaps
  // via a more comprehensive solution.
  static constexpr char16_t kEloRegexPattern[] =
      // clang-format off
    u"^("
      u"50("
        u"67("
          u"0[78]|"
          u"1[5789]|"
          u"2[012456789]|"
          u"3[01234569]|"
          u"4[0-7]|"
          u"53|"
          u"7[4-8]"
        u")|"
        u"9("
          u"0(0[0-9]|1[34]|2[013457]|3[0359]|4[0123568]|5[01456789]|6[012356789]|7[013]|8[123789]|9[1379])|"
          u"1(0[34568]|4[6-9]|5[1245678]|8[36789])|"
          u"2(2[02]|57|6[0-9]|7[1245689]|8[023456789]|9[1-6])|"
          u"3(0[78]|5[78])|"
          u"4(0[7-9]|1[012456789]|2[02]|5[7-9]|6[0-5]|8[45])|"
          u"55[01]|"
          u"636|"
          u"7(2[3-8]|32|6[5-9])"
        u")"
      u")|"
      u"4("
        u"0117[89]|3(1274|8935)|5(1416|7(393|63[12]))"
      u")|"
      u"6("
        u"27780|"
        u"36368|"
        u"5("
          u"0("
            u"0(3[1258]|4[026]|69|7[0178])|"
            u"4(0[67]|1[0-3]|2[2345689]|3[0568]|8[5-9]|9[0-9])|"
            u"5(0[01346789]|1[01246789]|2[0-9]|3[0178]|5[2-9]|6[012356789]|7[01789]|8[0134679]|9[0-8])|"
            u"72[0-7]|"
            u"9(0[1-9]|1[0-9]|2[0128]|3[89]|4[6-9]|5[01578]|6[2-9]|7[01])"
          u")|"
          u"16("
            u"5[236789]|"
            u"6[025678]|"
            u"7[013456789]|"
            u"88"
          u")|"
          u"50("
            u"0[01356789]|"
            u"1[2568]|"
            u"26|"
            u"3[6-8]|"
            u"5[1267]"
          u")"
        u")"
      u")"
    u")$";
  // clang-format on

  // Check for prefixes of length 6.
  if (stripped_number.size() >= 6) {
    int first_six_digits = 0;
    if (!base::StringToInt(stripped_number.substr(0, 6), &first_six_digits))
      return kGenericCard;

    if (MatchesRegex<kEloRegexPattern>(
            base::NumberToString16(first_six_digits))) {
      return kEloCard;
    }

    if (((first_six_digits >= 506099 && first_six_digits <= 506198) ||
         (first_six_digits >= 507865 && first_six_digits <= 507964) ||
         (first_six_digits >= 650002 && first_six_digits <= 650027)) &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableVerveCardSupport)) {
      return kVerveCard;
    }
  }

  // Check for prefixes of length 5.
  if (stripped_number.size() >= 5) {
    int first_five_digits = 0;
    if (!base::StringToInt(stripped_number.substr(0, 5), &first_five_digits))
      return kGenericCard;

    if (first_five_digits == 22050 || first_five_digits == 22051 ||
        first_five_digits == 22052) {
      return kTroyCard;
    }
  }

  // Check for prefixes of length 4.
  if (stripped_number.size() >= 4) {
    int first_four_digits = 0;
    if (!base::StringToInt(stripped_number.substr(0, 4), &first_four_digits))
      return kGenericCard;

    if (first_four_digits >= 2200 && first_four_digits <= 2204)
      return kMirCard;

    if (first_four_digits == 9792)
      return kTroyCard;

    if (first_four_digits >= 2221 && first_four_digits <= 2720)
      return kMasterCard;

    if (first_four_digits >= 3528 && first_four_digits <= 3589)
      return kJCBCard;

    if (first_four_digits == 6011)
      return kDiscoverCard;
  }

  // Check for prefixes of length 3.
  if (stripped_number.size() >= 3) {
    int first_three_digits = 0;
    if (!base::StringToInt(stripped_number.substr(0, 3), &first_three_digits))
      return kGenericCard;

    if ((first_three_digits >= 300 && first_three_digits <= 305) ||
        first_three_digits == 309)
      return kDinersCard;

    if (first_three_digits >= 644 && first_three_digits <= 649)
      return kDiscoverCard;
  }

  // Check for prefixes of length 2.
  if (stripped_number.size() >= 2) {
    int first_two_digits = 0;
    if (!base::StringToInt(stripped_number.substr(0, 2), &first_two_digits))
      return kGenericCard;

    if (first_two_digits == 34 || first_two_digits == 37)
      return kAmericanExpressCard;

    if (first_two_digits == 36 || first_two_digits == 38 ||
        first_two_digits == 39)
      return kDinersCard;

    if (first_two_digits >= 51 && first_two_digits <= 55)
      return kMasterCard;

    if (first_two_digits == 62)
      return kUnionPay;

    if (first_two_digits == 65)
      return kDiscoverCard;
  }

  // Check for prefixes of length 1.
  if (stripped_number.empty())
    return kGenericCard;

  if (stripped_number[0] == '4')
    return kVisaCard;

  return kGenericCard;
}

}  // namespace autofill
