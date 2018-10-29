// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "base/guid.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_search.h"
#include "base/i18n/time_formatting.h"
#include "base/i18n/unicodestring.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metadata.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/dtfmtsym.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace autofill {

// Unicode characters used in card number obfuscation:
//  - 0x2022 - Bullet.
//  - 0x2006 - SIX-PER-EM SPACE (small space between bullets).
//  - 0x2060 - WORD-JOINER (makes obfuscated string undivisible).
const base::char16 kMidlineEllipsis[] = {
    0x2022, 0x2060, 0x2006, 0x2060, 0x2022, 0x2060, 0x2006, 0x2060, 0x2022,
    0x2060, 0x2006, 0x2060, 0x2022, 0x2060, 0x2006, 0x2060, 0};

namespace {

const base::char16 kCreditCardObfuscationSymbol = '*';

bool ConvertYear(const base::string16& year, int* num) {
  // If the |year| is empty, clear the stored value.
  if (year.empty()) {
    *num = 0;
    return true;
  }

  // Try parsing the |year| as a number.
  if (base::StringToInt(year, num))
    return true;

  *num = 0;
  return false;
}

base::string16 NetworkForFill(const std::string& network) {
  if (network == kAmericanExpressCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX);
  if (network == kDinersCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DINERS);
  if (network == kDiscoverCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DISCOVER);
  if (network == kEloCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_ELO);
  if (network == kJCBCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_JCB);
  if (network == kMasterCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD);
  if (network == kMirCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MIR);
  if (network == kUnionPay)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_UNION_PAY);
  if (network == kVisaCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);

  // If you hit this DCHECK, the above list of cases needs to be updated to
  // include a new card.
  DCHECK_EQ(kGenericCard, network);
  return base::string16();
}

// Returns the last four digits of the credit card |number| (fewer if there are
// not enough characters in |number|).
base::string16 GetLastFourDigits(const base::string16& number) {
  static const size_t kNumLastDigits = 4;

  base::string16 stripped = CreditCard::StripSeparators(number);
  if (stripped.size() <= kNumLastDigits)
    return stripped;

  return stripped.substr(stripped.size() - kNumLastDigits, kNumLastDigits);
}

}  // namespace

namespace internal {

base::string16 GetObfuscatedStringForCardDigits(const base::string16& digits) {
  base::string16 obfuscated_string = base::string16(kMidlineEllipsis) + digits;
  base::i18n::WrapStringWithLTRFormatting(&obfuscated_string);
  return obfuscated_string;
}

}  // namespace internal

CreditCard::CreditCard(const std::string& guid, const std::string& origin)
    : AutofillDataModel(guid, origin),
      record_type_(LOCAL_CARD),
      card_type_(CARD_TYPE_UNKNOWN),
      network_(kGenericCard),
      expiration_month_(0),
      expiration_year_(0),
      server_status_(OK) {}

CreditCard::CreditCard(RecordType type, const std::string& server_id)
    : CreditCard() {
  DCHECK(type == MASKED_SERVER_CARD || type == FULL_SERVER_CARD);
  record_type_ = type;
  server_id_ = server_id;
}

CreditCard::CreditCard() : CreditCard(base::GenerateGUID(), std::string()) {}

CreditCard::CreditCard(const CreditCard& credit_card) : CreditCard() {
  operator=(credit_card);
}

CreditCard::~CreditCard() {}

// static
const base::string16 CreditCard::StripSeparators(const base::string16& number) {
  base::string16 stripped;
  base::RemoveChars(number, ASCIIToUTF16("- "), &stripped);
  return stripped;
}

// static
base::string16 CreditCard::NetworkForDisplay(const std::string& network) {
  if (kGenericCard == network)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_GENERIC);
  if (kAmericanExpressCard == network)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX_SHORT);

  return ::autofill::NetworkForFill(network);
}

// static
int CreditCard::IconResourceId(const std::string& network) {
  if (network == kAmericanExpressCard)
    return IDR_AUTOFILL_CC_AMEX;
  if (network == kDinersCard)
    return IDR_AUTOFILL_CC_DINERS;
  if (network == kDiscoverCard)
    return IDR_AUTOFILL_CC_DISCOVER;
  if (network == kEloCard)
    return IDR_AUTOFILL_CC_ELO;
  if (network == kJCBCard)
    return IDR_AUTOFILL_CC_JCB;
  if (network == kMasterCard)
    return IDR_AUTOFILL_CC_MASTERCARD;
  if (network == kMirCard)
    return IDR_AUTOFILL_CC_MIR;
  if (network == kUnionPay)
    return IDR_AUTOFILL_CC_UNIONPAY;
  if (network == kVisaCard)
    return IDR_AUTOFILL_CC_VISA;

  // If you hit this DCHECK, the above list of cases needs to be updated to
  // include a new card.
  DCHECK_EQ(kGenericCard, network);
  return IDR_AUTOFILL_CC_GENERIC;
}

// static
const char* CreditCard::GetCardNetwork(const base::string16& number) {
  // Credit card number specifications taken from:
  // https://en.wikipedia.org/wiki/Payment_card_number,
  // http://www.regular-expressions.info/creditcard.html,
  // https://developer.ean.com/general-info/valid-card-types,
  // http://www.bincodes.com/, and
  // http://www.fraudpractice.com/FL-binCC.html.
  // (Last updated: May 29, 2017)
  //
  // Card Type              Prefix(es)                                  Length
  // --------------------------------------------------------------------------
  // Visa                   4                                          13,16,19
  // American Express       34,37                                      15
  // Diners Club            300-305,309,36,38-39                       14
  // Discover Card          6011,644-649,65                            16
  // Elo                    431274,451416,5067,5090,627780,636297      16
  // JCB                    3528-3589                                  16
  // Mastercard             2221-2720, 51-55                           16
  // MIR                    2200-2204                                  16
  // UnionPay               62                                         16-19

  // Determine the network for the given |number| by going from the longest
  // (most specific) prefix to the shortest (most general) prefix.
  base::string16 stripped_number = CreditCard::StripSeparators(number);

  // Check for prefixes of length 6.
  if (stripped_number.size() >= 6) {
    int first_six_digits = 0;
    if (!base::StringToInt(stripped_number.substr(0, 6), &first_six_digits))
      return kGenericCard;

    if (first_six_digits == 431274 || first_six_digits == 451416 ||
        first_six_digits == 627780 || first_six_digits == 636297)
      return kEloCard;
  }

  // Check for prefixes of length 4.
  if (stripped_number.size() >= 4) {
    int first_four_digits = 0;
    if (!base::StringToInt(stripped_number.substr(0, 4), &first_four_digits))
      return kGenericCard;

    if (first_four_digits >= 2200 && first_four_digits <= 2204)
      return kMirCard;

    if (first_four_digits >= 2221 && first_four_digits <= 2720)
      return kMasterCard;

    if (first_four_digits >= 3528 && first_four_digits <= 3589)
      return kJCBCard;

    if (first_four_digits == 5067 || first_four_digits == 5090)
      return kEloCard;

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

void CreditCard::SetNetworkForMaskedCard(base::StringPiece network) {
  DCHECK_EQ(MASKED_SERVER_CARD, record_type());
  network_ = network.as_string();
}

void CreditCard::SetServerStatus(ServerStatus status) {
  DCHECK_NE(LOCAL_CARD, record_type());
  server_status_ = status;
}

CreditCard::ServerStatus CreditCard::GetServerStatus() const {
  DCHECK_NE(LOCAL_CARD, record_type());
  return server_status_;
}

AutofillMetadata CreditCard::GetMetadata() const {
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = (record_type_ == LOCAL_CARD ? guid() : server_id_);
  metadata.billing_address_id = billing_address_id_;
  return metadata;
}

bool CreditCard::SetMetadata(const AutofillMetadata metadata) {
  // Make sure the ids matches.
  if (metadata.id != (record_type_ == LOCAL_CARD ? guid() : server_id_))
    return false;

  if (!AutofillDataModel::SetMetadata(metadata))
    return false;

  billing_address_id_ = metadata.billing_address_id;
  return true;
}

base::string16 CreditCard::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(CREDIT_CARD, AutofillType(type).group());
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
      return name_on_card_;

    case CREDIT_CARD_NAME_FIRST:
      return data_util::SplitName(name_on_card_).given;

    case CREDIT_CARD_NAME_LAST:
      return data_util::SplitName(name_on_card_).family;

    case CREDIT_CARD_EXP_MONTH:
      return ExpirationMonthAsString();

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return Expiration2DigitYearAsString();

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return Expiration4DigitYearAsString();

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      base::string16 month = ExpirationMonthAsString();
      base::string16 year = Expiration2DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return base::string16();
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      base::string16 month = ExpirationMonthAsString();
      base::string16 year = Expiration4DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return base::string16();
    }

    case CREDIT_CARD_TYPE:
      return NetworkForFill();

    case CREDIT_CARD_NUMBER:
      return number_;

    case CREDIT_CARD_VERIFICATION_CODE:
      // Chrome doesn't store credit card verification codes.
      return base::string16();

    default:
      // ComputeDataPresentForArray will hit this repeatedly.
      return base::string16();
  }
}

void CreditCard::SetRawInfo(ServerFieldType type,
                            const base::string16& value) {
  DCHECK_EQ(CREDIT_CARD, AutofillType(type).group());
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
      name_on_card_ = value;
      break;

    case CREDIT_CARD_NAME_FIRST:
      temp_card_first_name_ = value;
      if (!temp_card_last_name_.empty()) {
        SetNameOnCardFromSeparateParts();
      }
      break;

    case CREDIT_CARD_NAME_LAST:
      temp_card_last_name_ = value;
      if (!temp_card_first_name_.empty()) {
        SetNameOnCardFromSeparateParts();
      }
      break;

    case CREDIT_CARD_EXP_MONTH:
      SetExpirationMonthFromString(value, std::string());
      break;

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      SetExpirationYearFromString(value);
      break;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      SetExpirationYearFromString(value);
      break;

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      SetExpirationDateFromString(value);
      break;

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      SetExpirationDateFromString(value);
      break;

    case CREDIT_CARD_TYPE:
      // This is a read-only attribute, determined by the credit card number.
      break;

    case CREDIT_CARD_NUMBER: {
      // Don't change the real value if the input is an obfuscated string.
      if (value.size() > 0 && value[0] != kCreditCardObfuscationSymbol)
        SetNumber(value);
      break;
    }

    case CREDIT_CARD_VERIFICATION_CODE:
      // Chrome doesn't store the credit card verification code.
      break;

    default:
      NOTREACHED() << "Attempting to set unknown info-type " << type;
      break;
  }
}

void CreditCard::GetMatchingTypes(const base::string16& text,
                                  const std::string& app_locale,
                                  ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  base::string16 card_number =
      GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale);
  if (!card_number.empty()) {
    // We only have the last four digits for masked cards, so match against
    // that if |this| is a masked card.
    bool numbers_match = record_type_ == MASKED_SERVER_CARD
                             ? GetLastFourDigits(text) == LastFourDigits()
                             : StripSeparators(text) == card_number;
    if (numbers_match)
      matching_types->insert(CREDIT_CARD_NUMBER);
  }

  int month;
  if (ConvertMonth(text, app_locale, &month) &&
      month == expiration_month_) {
    matching_types->insert(CREDIT_CARD_EXP_MONTH);
  }
}

void CreditCard::SetInfoForMonthInputType(const base::string16& value) {
  // Check if |text| is "yyyy-mm" format first, and check normal month format.
  if (!MatchesPattern(value, base::UTF8ToUTF16("^[0-9]{4}-[0-9]{1,2}$")))
    return;

  std::vector<base::StringPiece16> year_month = base::SplitStringPiece(
      value, ASCIIToUTF16("-"), base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(2u, year_month.size());
  int num = 0;
  bool converted = false;
  converted = base::StringToInt(year_month[0], &num);
  DCHECK(converted);
  SetExpirationYear(num);
  converted = base::StringToInt(year_month[1], &num);
  DCHECK(converted);
  SetExpirationMonth(num);
}

void CreditCard::SetExpirationMonth(int expiration_month) {
  if (expiration_month < 0 || expiration_month > 12)
    return;
  expiration_month_ = expiration_month;
}

void CreditCard::SetExpirationYear(int expiration_year) {
  // If |expiration_year| is beyond this millenium, or more than 2 digits but
  // before the current millenium (e.g. "545", "1995"), return. What is left are
  // values like "45" or "2018".
  if (expiration_year > 2999 ||
      (expiration_year > 99 && expiration_year < 2000))
    return;

  // Will normalize 2-digit years to the 4-digit version.
  if (expiration_year > 0 && expiration_year < 100) {
    base::Time::Exploded now_exploded;
    AutofillClock::Now().LocalExplode(&now_exploded);
    expiration_year += (now_exploded.year / 100) * 100;
  }

  expiration_year_ = expiration_year;
}

void CreditCard::operator=(const CreditCard& credit_card) {
  set_use_count(credit_card.use_count());
  set_use_date(credit_card.use_date());
  set_modification_date(credit_card.modification_date());

  if (this == &credit_card)
    return;

  record_type_ = credit_card.record_type_;
  card_type_ = credit_card.card_type_;
  number_ = credit_card.number_;
  name_on_card_ = credit_card.name_on_card_;
  network_ = credit_card.network_;
  expiration_month_ = credit_card.expiration_month_;
  expiration_year_ = credit_card.expiration_year_;
  server_id_ = credit_card.server_id_;
  server_status_ = credit_card.server_status_;
  billing_address_id_ = credit_card.billing_address_id_;
  bank_name_ = credit_card.bank_name_;
  temp_card_first_name_ = credit_card.temp_card_first_name_;
  temp_card_last_name_ = credit_card.temp_card_last_name_;

  set_guid(credit_card.guid());
  set_origin(credit_card.origin());
}

bool CreditCard::UpdateFromImportedCard(const CreditCard& imported_card,
                                        const std::string& app_locale) {
  if (this->GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale) !=
          imported_card.GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale)) {
    return false;
  }

  // Heuristically aggregated data should never overwrite verified data, with
  // the exception of expired verified cards. Instead, discard any heuristically
  // aggregated credit cards that disagree with explicitly entered data, so that
  // the UI is not cluttered with duplicate cards.
  if (this->IsVerified() && !imported_card.IsVerified()) {
    // If the original card is expired and the imported card is not, and the
    // name on the cards are identical, update the expiration date.
    if (this->IsExpired(AutofillClock::Now()) &&
        !imported_card.IsExpired(AutofillClock::Now()) &&
        (name_on_card_ == imported_card.name_on_card_)) {
      DCHECK(imported_card.expiration_month_ && imported_card.expiration_year_);
      expiration_month_ = imported_card.expiration_month_;
      expiration_year_ = imported_card.expiration_year_;
    }
    return true;
  }

  set_origin(imported_card.origin());

  // Note that the card number is intentionally not updated, so as to preserve
  // any formatting (i.e. separator characters).  Since the card number is not
  // updated, there is no reason to update the card type, either.
  if (!imported_card.name_on_card_.empty())
    name_on_card_ = imported_card.name_on_card_;

  // The expiration date for |imported_card| should always be set.
  DCHECK(imported_card.expiration_month_ && imported_card.expiration_year_);
  expiration_month_ = imported_card.expiration_month_;
  expiration_year_ = imported_card.expiration_year_;

  return true;
}

int CreditCard::Compare(const CreditCard& credit_card) const {
  // The following CreditCard field types are the only types we store in the
  // WebDB so far, so we're only concerned with matching these types in the
  // credit card.
  const ServerFieldType types[] = {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                   CREDIT_CARD_EXP_MONTH,
                                   CREDIT_CARD_EXP_4_DIGIT_YEAR};
  for (ServerFieldType type : types) {
    int comparison = GetRawInfo(type).compare(credit_card.GetRawInfo(type));
    if (comparison != 0)
      return comparison;
  }

  int comparison = server_id_.compare(credit_card.server_id_);
  if (comparison != 0)
    return comparison;

  comparison = billing_address_id_.compare(credit_card.billing_address_id_);
  if (comparison != 0)
    return comparison;

  comparison = bank_name_.compare(credit_card.bank_name_);
  if (comparison != 0)
    return comparison;

  if (static_cast<int>(server_status_) <
      static_cast<int>(credit_card.server_status_))
    return -1;
  if (static_cast<int>(server_status_) >
      static_cast<int>(credit_card.server_status_))
    return 1;
  if (static_cast<int>(record_type_) <
      static_cast<int>(credit_card.record_type_))
    return -1;
  if (static_cast<int>(record_type_) >
      static_cast<int>(credit_card.record_type_))
    return 1;
  return 0;
}

bool CreditCard::IsLocalDuplicateOfServerCard(const CreditCard& other) const {
  if (record_type() != LOCAL_CARD || other.record_type() == LOCAL_CARD)
    return false;

  // If |this| is only a partial card, i.e. some fields are missing, assume
  // those fields match.
  if ((!name_on_card_.empty() && name_on_card_ != other.name_on_card_) ||
      (expiration_month_ != 0 &&
       expiration_month_ != other.expiration_month_) ||
      (expiration_year_ != 0 && expiration_year_ != other.expiration_year_) ||
      (!billing_address_id_.empty() &&
       billing_address_id_ != other.billing_address_id_)) {
    return false;
  }

  if (number_.empty())
    return true;

  return HasSameNumberAs(other);
}

bool CreditCard::HasSameNumberAs(const CreditCard& other) const {
  // For masked cards, this is the best we can do to compare card numbers.
  if (record_type() == MASKED_SERVER_CARD ||
      other.record_type() == MASKED_SERVER_CARD) {
    return NetworkAndLastFourDigits() == other.NetworkAndLastFourDigits();
  }

  return StripSeparators(number_) == StripSeparators(other.number_);
}

bool CreditCard::operator==(const CreditCard& credit_card) const {
  return guid() == credit_card.guid() &&
         origin() == credit_card.origin() &&
         Compare(credit_card) == 0;
}

bool CreditCard::operator!=(const CreditCard& credit_card) const {
  return !operator==(credit_card);
}

bool CreditCard::IsEmpty(const std::string& app_locale) const {
  ServerFieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

bool CreditCard::IsValid() const {
  return HasValidCardNumber() && HasValidExpirationDate();
}

bool CreditCard::HasValidCardNumber() const {
  return IsValidCreditCardNumber(number_);
}

bool CreditCard::HasValidExpirationDate() const {
  return IsValidCreditCardExpirationDate(expiration_year_, expiration_month_,
                                         AutofillClock::Now());
}

bool CreditCard::SetExpirationMonthFromString(const base::string16& text,
                                              const std::string& app_locale) {
  base::string16 trimmed;
  base::TrimWhitespace(text, base::TRIM_ALL, &trimmed);

  int month = 0;
  if (!ConvertMonth(trimmed, app_locale, &month))
    return false;

  SetExpirationMonth(month);
  return true;
}

void CreditCard::SetExpirationYearFromString(const base::string16& text) {
  int year;
  if (!ConvertYear(text, &year))
    return;

  SetExpirationYear(year);
}

void CreditCard::SetExpirationDateFromString(const base::string16& text) {
  // Check that |text| fits the supported patterns: mmyy, mmyyyy, m-yy,
  // mm-yy, m-yyyy and mm-yyyy. Note that myy and myyyy matched by this pattern
  // but are not supported (ambiguous). Separators: -, / and |.
  if (!MatchesPattern(text, base::UTF8ToUTF16("^[0-9]{1,2}[-/|]?[0-9]{2,4}$")))
    return;

  base::string16 month;
  base::string16 year;

  // Check for a separator.
  base::string16 found_separator;
  const std::vector<base::string16> kSeparators{
      ASCIIToUTF16("-"), ASCIIToUTF16("/"), ASCIIToUTF16("|")};
  for (const base::string16& separator : kSeparators) {
    if (text.find(separator) != base::string16::npos) {
      found_separator = separator;
      break;
    }
  }

  if (!found_separator.empty()) {
    std::vector<base::string16> month_year = base::SplitString(
        text, found_separator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    DCHECK_EQ(2u, month_year.size());
    month = month_year[0];
    year = month_year[1];
  } else if (text.size() % 2 == 0) {
    // If there are no separators, the supported formats are mmyy and mmyyyy.
    month = text.substr(0, 2);
    year = text.substr(2);
  } else {
    // Odd number of digits with no separator is too ambiguous.
    return;
  }

  int num = 0;
  bool converted = false;
  converted = base::StringToInt(month, &num);
  DCHECK(converted);
  SetExpirationMonth(num);
  converted = base::StringToInt(year, &num);
  DCHECK(converted);
  SetExpirationYear(num);
}

const std::pair<base::string16, base::string16> CreditCard::LabelPieces()
    const {
  base::string16 label;
  // No CC number, return name only.
  if (number().empty())
    return std::make_pair(name_on_card_, base::string16());

  base::string16 obfuscated_cc_number = NetworkOrBankNameAndLastFourDigits();
  // No expiration date set.
  if (!expiration_month_ || !expiration_year_)
    return std::make_pair(obfuscated_cc_number, base::string16());

  base::string16 formatted_date = ExpirationDateForDisplay();

  base::string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
  return std::make_pair(obfuscated_cc_number, separator + formatted_date);
}

const base::string16 CreditCard::Label() const {
  std::pair<base::string16, base::string16> pieces = LabelPieces();
  return pieces.first + pieces.second;
}

base::string16 CreditCard::LastFourDigits() const {
  return GetLastFourDigits(number_);
}

base::string16 CreditCard::NetworkForDisplay() const {
  return CreditCard::NetworkForDisplay(network_);
}

base::string16 CreditCard::ObfuscatedLastFourDigits() const {
  return internal::GetObfuscatedStringForCardDigits(LastFourDigits());
}

base::string16 CreditCard::NetworkAndLastFourDigits() const {
  const base::string16 network = NetworkForDisplay();
  // TODO(crbug.com/734197): truncate network.

  const base::string16 digits = LastFourDigits();
  if (digits.empty())
    return network;

  // TODO(estade): i18n?
  const base::string16 obfuscated_string =
      internal::GetObfuscatedStringForCardDigits(digits);
  return network.empty() ? obfuscated_string
                         : network + ASCIIToUTF16("  ") + obfuscated_string;
}

base::string16 CreditCard::BankNameAndLastFourDigits() const {
  const base::string16 digits = LastFourDigits();
  // TODO(crbug.com/734197): truncate bank name.
  if (digits.empty())
    return ASCIIToUTF16(bank_name_);

  const base::string16 obfuscated_string =
      internal::GetObfuscatedStringForCardDigits(digits);
  return bank_name_.empty() ? obfuscated_string
                            : ASCIIToUTF16(bank_name_) + ASCIIToUTF16("  ") +
                                  obfuscated_string;
}

base::string16 CreditCard::NetworkOrBankNameAndLastFourDigits() const {
  return bank_name_.empty() ? NetworkAndLastFourDigits()
                            : BankNameAndLastFourDigits();
}

base::string16
CreditCard::NetworkOrBankNameLastFourDigitsAndDescriptiveExpiration(
    const std::string& app_locale) const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_TWO_LINE_LABEL_FROM_NAME,
      NetworkOrBankNameAndLastFourDigits(),
      GetInfo(AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale));
}

base::string16 CreditCard::DescriptiveExpiration(
    const std::string& app_locale) const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_TWO_LINE_LABEL_FROM_CARD_NUMBER,
      GetInfo(AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale));
}

base::string16 CreditCard::AbbreviatedExpirationDateForDisplay(
    bool with_prefix) const {
  base::string16 month = ExpirationMonthAsString();
  base::string16 year = Expiration2DigitYearAsString();
  if (month.empty() || year.empty())
    return base::string16();

  return l10n_util::GetStringFUTF16(
      with_prefix ? IDS_AUTOFILL_CREDIT_CARD_EXPIRATION_DATE_ABBR
                  : IDS_AUTOFILL_CREDIT_CARD_EXPIRATION_DATE_ABBR_V2,
      month, year);
}

base::string16 CreditCard::ExpirationDateForDisplay() const {
  base::string16 formatted_date(ExpirationMonthAsString());
  formatted_date.append(ASCIIToUTF16("/"));
  formatted_date.append(Expiration4DigitYearAsString());
  return formatted_date;
}

base::string16 CreditCard::ExpirationMonthAsString() const {
  if (expiration_month_ == 0)
    return base::string16();

  base::string16 month = base::IntToString16(expiration_month_);
  if (expiration_month_ >= 10)
    return month;

  base::string16 zero = ASCIIToUTF16("0");
  zero.append(month);
  return zero;
}

base::string16 CreditCard::Expiration4DigitYearAsString() const {
  if (expiration_year_ == 0)
    return base::string16();

  return base::IntToString16(Expiration4DigitYear());
}

bool CreditCard::HasFirstAndLastName() const {
  return !temp_card_first_name_.empty() && !temp_card_last_name_.empty() &&
         !name_on_card_.empty();
}

base::string16 CreditCard::Expiration2DigitYearAsString() const {
  if (expiration_year_ == 0)
    return base::string16();

  return base::IntToString16(Expiration2DigitYear());
}

void CreditCard::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(CREDIT_CARD_NAME_FULL);
  supported_types->insert(CREDIT_CARD_NAME_FIRST);
  supported_types->insert(CREDIT_CARD_NAME_LAST);
  supported_types->insert(CREDIT_CARD_NUMBER);
  supported_types->insert(CREDIT_CARD_TYPE);
  supported_types->insert(CREDIT_CARD_EXP_MONTH);
  supported_types->insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
}

base::string16 CreditCard::GetInfoImpl(const AutofillType& type,
                                       const std::string& app_locale) const {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == CREDIT_CARD_NUMBER) {
    // Web pages should never actually be filled by a masked server card,
    // but this function is used at the preview stage.
    if (record_type() == MASKED_SERVER_CARD)
      return NetworkOrBankNameAndLastFourDigits();

    return StripSeparators(number_);
  }

  return GetRawInfo(storable_type);
}

bool CreditCard::SetInfoImpl(const AutofillType& type,
                             const base::string16& value,
                             const std::string& app_locale) {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == CREDIT_CARD_EXP_MONTH)
    return SetExpirationMonthFromString(value, app_locale);

  if (storable_type == CREDIT_CARD_NUMBER)
    SetRawInfo(storable_type, StripSeparators(value));
  else
    SetRawInfo(storable_type, value);
  return true;
}

base::string16 CreditCard::NetworkForFill() const {
  return ::autofill::NetworkForFill(network_);
}

void CreditCard::SetNumber(const base::string16& number) {
  number_ = number;

  // Set the type based on the card number, but only for full numbers, not
  // when we have masked cards from the server (last 4 digits).
  if (record_type_ != MASKED_SERVER_CARD)
    network_ = GetCardNetwork(StripSeparators(number_));
}

void CreditCard::RecordAndLogUse() {
  UMA_HISTOGRAM_COUNTS_1000("Autofill.DaysSinceLastUse.CreditCard",
                            (AutofillClock::Now() - use_date()).InDays());
  RecordUse();
}

// static
bool CreditCard::ConvertMonth(const base::string16& month,
                              const std::string& app_locale,
                              int* num) {
  if (month.empty())
    return false;

  // Try parsing the |month| as a number (this doesn't require |app_locale|).
  if (base::StringToInt(month, num))
    return true;

  if (app_locale.empty())
    return false;

  // Otherwise, try parsing the |month| as a named month, e.g. "January" or
  // "Jan" in the user's locale.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale(app_locale.c_str());
  icu::DateFormatSymbols date_format_symbols(locale, status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);
  // Full months (January, Janvier, etc.)
  int32_t num_months;
  const icu::UnicodeString* months = date_format_symbols.getMonths(num_months);
  for (int32_t i = 0; i < num_months; ++i) {
    const base::string16 icu_month(
        base::i18n::UnicodeStringToString16(months[i]));
    // We look for the ICU-defined month in |month|.
    if (base::i18n::StringSearchIgnoringCaseAndAccents(icu_month, month,
                                                       nullptr, nullptr)) {
      *num = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return true;
    }
  }
  // Abbreviated months (jan., janv., fév.) Some abbreviations have . at the end
  // (e.g., "janv." in French). The period is removed.
  months = date_format_symbols.getShortMonths(num_months);
  base::string16 trimmed_month;
  base::TrimString(month, ASCIIToUTF16("."), &trimmed_month);
  for (int32_t i = 0; i < num_months; ++i) {
    base::string16 icu_month(base::i18n::UnicodeStringToString16(months[i]));
    base::TrimString(icu_month, ASCIIToUTF16("."), &icu_month);
    // We look for the ICU-defined month in |trimmed_month|.
    if (base::i18n::StringSearchIgnoringCaseAndAccents(icu_month, trimmed_month,
                                                       nullptr, nullptr)) {
      *num = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return true;
    }
  }

  return false;
}

bool CreditCard::IsExpired(const base::Time& current_time) const {
  return !IsValidCreditCardExpirationDate(expiration_year_, expiration_month_,
                                          current_time);
}

bool CreditCard::ShouldUpdateExpiration(const base::Time& current_time) const {
  // Local cards always have OK server status.
  DCHECK(server_status_ == OK || record_type_ != LOCAL_CARD);
  return server_status_ == EXPIRED || IsExpired(current_time);
}

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card) {
  return os << base::UTF16ToUTF8(credit_card.Label()) << " "
            << credit_card.guid() << " " << credit_card.origin() << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
            << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_TYPE))
            << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_NUMBER))
            << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_EXP_MONTH))
            << " " << base::UTF16ToUTF8(
                          credit_card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

void CreditCard::SetNameOnCardFromSeparateParts() {
  DCHECK(name_on_card_.empty() && !temp_card_first_name_.empty() &&
         !temp_card_last_name_.empty());
  name_on_card_ =
      temp_card_first_name_ + base::UTF8ToUTF16(" ") + temp_card_last_name_;
}

const char kAmericanExpressCard[] = "americanExpressCC";
const char kDinersCard[] = "dinersCC";
const char kDiscoverCard[] = "discoverCC";
const char kEloCard[] = "eloCC";
const char kGenericCard[] = "genericCC";
const char kJCBCard[] = "jcbCC";
const char kMasterCard[] = "masterCardCC";
const char kMirCard[] = "mirCC";
const char kUnionPay[] = "unionPayCC";
const char kVisaCard[] = "visaCC";

}  // namespace autofill
