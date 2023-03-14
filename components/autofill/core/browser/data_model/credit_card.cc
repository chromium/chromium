// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <ostream>
#include <string>

#include "base/check_op.h"
#include "base/guid.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace autofill {

// Unicode characters used in card number obfuscation:
//  - \u2022 - Bullet.
//  - \u2006 - SIX-PER-EM SPACE (small space between bullets).
//  - \u2060 - WORD-JOINER (makes obfuscated string undivisible).
constexpr char16_t kMidlineEllipsisDot[] = u"\u2022\u2060\u2006\u2060";
constexpr char16_t kMidlineEllipsisPlainDot = u'\u2022';

namespace {

const char16_t kCreditCardObfuscationSymbol = '*';

const char16_t kWhiteSpaceSeparator = ' ';

const int kMaxNicknameLength = 25;

constexpr std::array<int, 3> k15DigitAmexNumberSegmentations = {4, 6, 5};
constexpr std::array<int, 4> k16DigitNumberSegmentations = {4, 4, 4, 4};

// Suffix for GUID of a virtual card to differentiate it from it's corresponding
// masked server card..
const char kVirtualCardIdentifierSuffix[] = "_vcn";

std::u16string NetworkForFill(const std::string& network) {
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
  if (network == kTroyCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_TROY);
  if (network == kUnionPay)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_UNION_PAY);
  if (network == kVisaCard)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);

  // If you hit this DCHECK, the above list of cases needs to be updated to
  // include a new card.
  DCHECK_EQ(kGenericCard, network);
  return std::u16string();
}

// Returns the last four digits of the credit card |number| (fewer if there are
// not enough characters in |number|).
std::u16string GetLastFourDigits(const std::u16string& number) {
  static const size_t kNumLastDigits = 4;

  std::u16string stripped = CreditCard::StripSeparators(number);
  if (stripped.size() <= kNumLastDigits)
    return stripped;

  return stripped.substr(stripped.size() - kNumLastDigits, kNumLastDigits);
}

// Returns a new string based on the input `number` by adding a white space
// between `segments`. The provided `segments` denotes the length of each
// segment, and we don't need to add whitespace to the last segmentation. For
// example, if you would like to format 15-digit card number into "XXXX XXXXXX
// XXXXX", you need to provide [4, 6, 5] as the `segments`.
std::u16string AddWhiteSpaceSeparatorForNumber(const std::u16string& number,
                                               base::span<const int> segments) {
  std::u16string formatted;
  int pos = 0;
  // We don't need to add white space to the last segmentation.
  for (size_t i = 0; i < segments.size() - 1; i++) {
    formatted += number.substr(pos, segments[i]) + kWhiteSpaceSeparator;
    pos += segments[i];
  }
  // Add the remaining digits.
  formatted += number.substr(pos);
  return formatted;
}

}  // namespace

namespace internal {

std::u16string GetObfuscatedStringForCardDigits(const std::u16string& digits,
                                                int obfuscation_length) {
  std::u16string obfuscated_string =
      CreditCard::GetMidlineEllipsisDots(obfuscation_length);
  obfuscated_string.append(digits);
  base::i18n::WrapStringWithLTRFormatting(&obfuscated_string);
  return obfuscated_string;
}

}  // namespace internal

// static
CreditCard CreditCard::CreateVirtualCard(const CreditCard& card) {
  // Virtual cards can be created only from masked server cards.
  DCHECK_EQ(card.record_type(), MASKED_SERVER_CARD);
  CreditCard virtual_card = card;
  virtual_card.set_record_type(VIRTUAL_CARD);
  return virtual_card;
}

// static
std::unique_ptr<CreditCard> CreditCard::CreateVirtualCardWithGuidSuffix(
    const CreditCard& card) {
  // Virtual cards can be created only from masked server cards.
  DCHECK_EQ(card.record_type(), MASKED_SERVER_CARD);
  auto virtual_card = std::make_unique<CreditCard>(card);
  virtual_card->set_record_type(VIRTUAL_CARD);
  // Add a suffix to the guid to help differentiate the virtual card from the
  // server card.
  virtual_card->set_guid(card.guid() + kVirtualCardIdentifierSuffix);
  return virtual_card;
}

CreditCard::CreditCard(const std::string& guid, const std::string& origin)
    : AutofillDataModel(guid, origin),
      record_type_(LOCAL_CARD),
      network_(kGenericCard),
      expiration_month_(0),
      expiration_year_(0),
      card_issuer_(ISSUER_UNKNOWN),
      instrument_id_(0) {}

CreditCard::CreditCard(RecordType type, const std::string& server_id)
    : CreditCard() {
  DCHECK(type == MASKED_SERVER_CARD || type == FULL_SERVER_CARD);
  record_type_ = type;
  server_id_ = server_id;
}

CreditCard::CreditCard(RecordType type, int64_t instrument_id) : CreditCard() {
  DCHECK(type == MASKED_SERVER_CARD || type == FULL_SERVER_CARD);
  record_type_ = type;
  instrument_id_ = instrument_id;
}

CreditCard::CreditCard() : CreditCard(base::GenerateGUID(), std::string()) {}

CreditCard::CreditCard(const CreditCard& credit_card) = default;
CreditCard::CreditCard(CreditCard&& credit_card) = default;
CreditCard& CreditCard::operator=(const CreditCard& credit_card) = default;
CreditCard& CreditCard::operator=(CreditCard&& credit_card) = default;

CreditCard::~CreditCard() = default;

// static
const std::u16string CreditCard::StripSeparators(const std::u16string& number) {
  std::u16string stripped;
  base::RemoveChars(number, u"- ", &stripped);
  return stripped;
}

// static
std::u16string CreditCard::NetworkForDisplay(const std::string& network) {
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
  if (network == kTroyCard)
    return IDR_AUTOFILL_CC_TROY;
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
const char* CreditCard::GetCardNetwork(const std::u16string& number) {
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

  // Determine the network for the given |number| by going from the longest
  // (most specific) prefix to the shortest (most general) prefix.
  std::u16string stripped_number = CreditCard::StripSeparators(number);

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

    // This is a flag controlled rollout to update the way we recognize Elo BIN.
    if (base::FeatureList::IsEnabled(
            features::kAutofillUseEloRegexForBinMatching) &&
        MatchesRegex<kEloRegexPattern>(
            base::NumberToString16(first_six_digits))) {
      return kEloCard;
    }

    if (!base::FeatureList::IsEnabled(
            features::kAutofillUseEloRegexForBinMatching) &&
        (first_six_digits == 431274 || first_six_digits == 451416 ||
         first_six_digits == 627780 || first_six_digits == 636297)) {
      return kEloCard;
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

    if (!base::FeatureList::IsEnabled(
            features::kAutofillUseEloRegexForBinMatching) &&
        (first_four_digits == 5067 || first_four_digits == 5090)) {
      return kEloCard;
    }

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

// static
bool CreditCard::IsNicknameValid(const std::u16string& nickname) {
  // Must not exceed max length.
  if (nickname.size() > kMaxNicknameLength)
    return false;

  // Must not contain newlines, tabs, or carriage returns.
  if (nickname.find('\n') != std::u16string::npos ||
      nickname.find('\r') != std::u16string::npos ||
      nickname.find('\t') != std::u16string::npos) {
    return false;
  }

  // Must not contain digits.
  for (char16_t c : nickname) {
    if (base::IsAsciiDigit(c))
      return false;
  }

  return true;
}

// static
std::u16string CreditCard::GetMidlineEllipsisDots(size_t num_dots) {
  DCHECK(num_dots > 0);
  std::u16string dots;
  dots.reserve(sizeof(kMidlineEllipsisDot) * num_dots);

  for (size_t i = 0; i < num_dots; i++) {
    dots.append(kMidlineEllipsisDot);
  }
  return dots;
}

// static
bool CreditCard::IsLocalCard(const CreditCard* card) {
  return card && card->record_type() == CreditCard::LOCAL_CARD;
}

void CreditCard::SetNetworkForMaskedCard(base::StringPiece network) {
  DCHECK_EQ(MASKED_SERVER_CARD, record_type());
  network_ = std::string(network);
}

AutofillMetadata CreditCard::GetMetadata() const {
  AutofillMetadata metadata = AutofillDataModel::GetMetadata();
  metadata.id = (record_type_ == LOCAL_CARD ? guid() : server_id_);
  metadata.billing_address_id = billing_address_id_;
  return metadata;
}

double CreditCard::GetRankingScore(base::Time current_time) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableRankingFormulaCreditCards)) {
    int virtual_card_boost =
        virtual_card_enrollment_state_ != VirtualCardEnrollmentState::ENROLLED
            ? 0
            : features::kAutofillRankingFormulaVirtualCardBoost.Get() *
                  exp(-GetDaysSinceLastUse(current_time) /
                      features::kAutofillRankingFormulaVirtualCardBoostHalfLife
                          .Get());

    // Exponentially decay the use count by the days since the data model was
    // last used. Add a virtual card boost if the model is a virtual card.
    return (log10(use_count() + 1) *
            exp(-GetDaysSinceLastUse(current_time) /
                features::kAutofillRankingFormulaCreditCardsUsageHalfLife
                    .Get())) +
           virtual_card_boost;
  }

  // Default to legacy frecency scoring.
  return AutofillDataModel::GetRankingScore(current_time);
}

bool CreditCard::SetMetadata(const AutofillMetadata& metadata) {
  // Make sure the ids matches.
  if (metadata.id != (record_type_ == LOCAL_CARD ? guid() : server_id_))
    return false;

  if (!AutofillDataModel::SetMetadata(metadata))
    return false;

  billing_address_id_ = metadata.billing_address_id;
  return true;
}

bool CreditCard::IsDeletable() const {
  return AutofillDataModel::IsDeletable() &&
         IsExpired(AutofillClock::Now() - kDisusedDataModelDeletionTimeDelta);
}

std::u16string CreditCard::GetRawInfo(ServerFieldType type) const {
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
      return name_on_card_;

    case CREDIT_CARD_NAME_FIRST:
      return data_util::SplitName(name_on_card_).given;

    case CREDIT_CARD_NAME_LAST:
      return data_util::SplitName(name_on_card_).family;

    case CREDIT_CARD_EXP_MONTH:
      return Expiration2DigitMonthAsString();

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return Expiration2DigitYearAsString();

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return Expiration4DigitYearAsString();

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      std::u16string month = Expiration2DigitMonthAsString();
      std::u16string year = Expiration2DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + u"/" + year;
      return std::u16string();
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      std::u16string month = Expiration2DigitMonthAsString();
      std::u16string year = Expiration4DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + u"/" + year;
      return std::u16string();
    }

    case CREDIT_CARD_TYPE:
      return NetworkForFill();

    case CREDIT_CARD_NUMBER:
      return number_;

    case CREDIT_CARD_VERIFICATION_CODE:
      // Chrome doesn't store credit card verification codes.
      return std::u16string();

    default:
      // ComputeDataPresentForArray will hit this repeatedly.
      return std::u16string();
  }
}

void CreditCard::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                                  const std::u16string& value,
                                                  VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kCreditCard, AutofillType(type).group());
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

void CreditCard::GetMatchingTypes(const std::u16string& text,
                                  const std::string& app_locale,
                                  ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  std::u16string card_number =
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

  int month = 0;
  if (data_util::ParseExpirationMonth(text, app_locale, &month) &&
      month == expiration_month_) {
    matching_types->insert(CREDIT_CARD_EXP_MONTH);
  }
}

void CreditCard::SetInfoForMonthInputType(const std::u16string& value) {
  static constexpr char16_t kDateRegex[] = u"^[0-9]{4}-[0-9]{1,2}$";
  // Check if |text| is "yyyy-mm" format first, and check normal month format.
  if (!MatchesRegex<kDateRegex>(value))
    return;

  std::vector<base::StringPiece16> year_month = base::SplitStringPiece(
      value, u"-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
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
  data_util::SetExpirationMonth(expiration_month, &expiration_month_);
}

void CreditCard::SetExpirationYear(int expiration_year) {
  data_util::SetExpirationYear(expiration_year, &expiration_year_);
}

void CreditCard::SetNickname(const std::u16string& nickname) {
  // First replace all tabs and newlines with whitespaces and store it as
  // |nickname_|.
  base::ReplaceChars(nickname, u"\t\r\n", u" ", &nickname_);
  // Then trim leading/trailing whitespaces from |nickname_|.
  base::TrimString(nickname_, u" ", &nickname_);
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
    // name on the cards are identical, and the imported card's expiration date
    // is not empty, update the expiration date.
    if (this->IsExpired(AutofillClock::Now()) &&
        !imported_card.IsExpired(AutofillClock::Now()) &&
        (name_on_card_ == imported_card.name_on_card_) &&
        (imported_card.expiration_month_ && imported_card.expiration_year_)) {
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

  // If |imported_card| has an expiration date, overwrite |this|'s expiration
  // date with its value.
  if (imported_card.expiration_month_ && imported_card.expiration_year_) {
    expiration_month_ = imported_card.expiration_month_;
    expiration_year_ = imported_card.expiration_year_;
  }

  return true;
}

int CreditCard::Compare(const CreditCard& credit_card) const {
  // The following CreditCard field types are the only types we store in the
  // WebDB so far, so we're only concerned with matching these types in the
  // credit card.
  const ServerFieldType types[] = {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                                   CREDIT_CARD_EXP_4_DIGIT_YEAR};
  for (ServerFieldType type : types) {
    int comparison = GetRawInfo(type).compare(credit_card.GetRawInfo(type));
    if (comparison != 0)
      return comparison;
  }

  if (!HasSameNumberAs(credit_card)) {
    return number().compare(credit_card.number());
  }

  int comparison = server_id_.compare(credit_card.server_id_);
  if (comparison != 0)
    return comparison;

  comparison = billing_address_id_.compare(credit_card.billing_address_id_);
  if (comparison != 0)
    return comparison;

  comparison = nickname_.compare(credit_card.nickname_);
  if (comparison != 0)
    return comparison;

  comparison = product_description_.compare(credit_card.product_description_);
  if (comparison != 0)
    return comparison;

  if (static_cast<int>(card_issuer_) <
      static_cast<int>(credit_card.card_issuer_)) {
    return -1;
  }
  if (static_cast<int>(card_issuer_) >
      static_cast<int>(credit_card.card_issuer_)) {
    return 1;
  }

  comparison = issuer_id_.compare(credit_card.issuer_id_);
  if (comparison != 0)
    return comparison;

  if (static_cast<int>(virtual_card_enrollment_state_) <
      static_cast<int>(credit_card.virtual_card_enrollment_state_)) {
    return -1;
  }
  if (static_cast<int>(virtual_card_enrollment_state_) >
      static_cast<int>(credit_card.virtual_card_enrollment_state_)) {
    return 1;
  }

  if (virtual_card_enrollment_type_ <
      credit_card.virtual_card_enrollment_type_) {
    return -1;
  }
  if (virtual_card_enrollment_type_ >
      credit_card.virtual_card_enrollment_type_) {
    return 1;
  }

  comparison = card_art_url_.spec().compare(credit_card.card_art_url_.spec());
  if (comparison != 0)
    return comparison;

  // Do not distinguish masked server cards from full server cards as this is
  // not needed and not desired - we want to identify masked server card from
  // sync with the (potential) full server card stored locally.
  if (record_type_ == LOCAL_CARD && credit_card.record_type_ != LOCAL_CARD)
    return -1;
  if (record_type_ != LOCAL_CARD && credit_card.record_type_ == LOCAL_CARD)
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
  // Masked cards are considered to have the same number if their last four
  // digits match and if any expiration date information available for both
  // cards matches.
  if (record_type() == MASKED_SERVER_CARD ||
      other.record_type() == MASKED_SERVER_CARD) {
    bool last_four_digits_match = LastFourDigits() == other.LastFourDigits();

    bool months_match = expiration_month() == other.expiration_month() ||
                        expiration_month() == 0 ||
                        other.expiration_month() == 0;

    bool years_match = expiration_year() == other.expiration_year() ||
                       expiration_year() == 0 || other.expiration_year() == 0;

    return last_four_digits_match && months_match && years_match;
  }

  return StripSeparators(number_) == StripSeparators(other.number_);
}

bool CreditCard::operator==(const CreditCard& credit_card) const {
  return guid() == credit_card.guid() && origin() == credit_card.origin() &&
         record_type() == credit_card.record_type() &&
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

bool CreditCard::HasValidExpirationYear() const {
  return IsValidCreditCardExpirationYear(expiration_year_,
                                         AutofillClock::Now());
}

bool CreditCard::HasValidExpirationDate() const {
  return IsValidCreditCardExpirationDate(expiration_year_, expiration_month_,
                                         AutofillClock::Now());
}

bool CreditCard::SetExpirationMonthFromString(const std::u16string& text,
                                              const std::string& app_locale) {
  return data_util::ParseExpirationMonth(text, app_locale, &expiration_month_);
}

bool CreditCard::SetExpirationYearFromString(const std::u16string& text) {
  return data_util::ParseExpirationYear(text, &expiration_year_);
}

void CreditCard::SetExpirationDateFromString(const std::u16string& text) {
  static constexpr char16_t kDateRegex[] =
      uR"(^\s*[0-9]{1,2}\s*[-/|]?\s*[0-9]{2,4}\s*$)";
  // Check that |text| fits the supported patterns: mmyy, mmyyyy, m-yy,
  // mm-yy, m-yyyy and mm-yyyy. Note that myy and myyyy matched by this pattern
  // but are not supported (ambiguous). Separators: -, / and |.
  if (!MatchesRegex<kDateRegex>(text))
    return;

  std::u16string month;
  std::u16string year;

  // Check for a separator.
  std::u16string found_separator;
  const std::vector<std::u16string> kSeparators{u"-", u"/", u"|"};
  for (const std::u16string& separator : kSeparators) {
    if (text.find(separator) != std::u16string::npos) {
      found_separator = separator;
      break;
    }
  }

  if (!found_separator.empty()) {
    std::vector<std::u16string> month_year = base::SplitString(
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

std::pair<std::u16string, std::u16string> CreditCard::LabelPieces() const {
  if (number().empty()) {
    // No CC number, if valid nickname is present, return nickname only.
    // Otherwise, return cardholder name only.
    if (HasNonEmptyValidNickname())
      return std::make_pair(nickname_, std::u16string());

    return std::make_pair(name_on_card_, std::u16string());
  }

  return std::make_pair(CardIdentifierStringForAutofillDisplay(),
                        name_on_card_);
}

std::u16string CreditCard::Label() const {
  std::pair<std::u16string, std::u16string> pieces = LabelPieces();
  if (pieces.first.empty() || pieces.second.empty())
    return pieces.first + pieces.second;

  return pieces.first + u", " + pieces.second;
}

std::u16string CreditCard::LastFourDigits() const {
  return GetLastFourDigits(number_);
}

std::u16string CreditCard::FullDigitsForDisplay() const {
  std::u16string stripped = CreditCard::StripSeparators(number_);
  if (stripped.size() == 16) {
    return AddWhiteSpaceSeparatorForNumber(stripped,
                                           k16DigitNumberSegmentations);
  }
  if (stripped.size() == 15 && network_ == kAmericanExpressCard) {
    return AddWhiteSpaceSeparatorForNumber(stripped,
                                           k15DigitAmexNumberSegmentations);
  }

  return number_;
}

std::u16string CreditCard::NetworkForDisplay() const {
  return CreditCard::NetworkForDisplay(network_);
}

std::u16string CreditCard::ObfuscatedNumberWithVisibleLastFourDigits(
    int obfuscation_length) const {
  return internal::GetObfuscatedStringForCardDigits(LastFourDigits(),
                                                    obfuscation_length);
}

std::u16string
CreditCard::ObfuscatedNumberWithVisibleLastFourDigitsForSplitFields() const {
  // For split credit card number fields, use plain dots without spacing and no
  // LTR formatting. Only obfuscate 12 dots and append the last four digits of
  // the credit card number.
  return std::u16string(12, kMidlineEllipsisPlainDot) + LastFourDigits();
}

std::string CreditCard::CardIconStringForAutofillSuggestion() const {
  return network_;
}

std::u16string CreditCard::NetworkAndLastFourDigits(
    int obfuscation_length) const {
  const std::u16string network = NetworkForDisplay();
  // TODO(crbug.com/734197): truncate network.

  const std::u16string digits = LastFourDigits();
  if (digits.empty())
    return network;

  // TODO(estade): i18n?
  const std::u16string obfuscated_string =
      internal::GetObfuscatedStringForCardDigits(digits, obfuscation_length);
  return network.empty() ? obfuscated_string
                         : network + u"  " + obfuscated_string;
}

// TODO(crbug.com/1357204): Rename to CardNameAndLastFourDigits.
std::u16string CreditCard::CardIdentifierStringForAutofillDisplay(
    std::u16string customized_nickname,
    int obfuscation_length) const {
  std::u16string card_name = CardNameForAutofillDisplay(customized_nickname);
  std::u16string last_four = LastFourDigits();

  if (last_four.empty())
    return card_name;

  std::u16string obfuscated_last_four =
      internal::GetObfuscatedStringForCardDigits(last_four, obfuscation_length);
  return card_name.empty()
             ? obfuscated_last_four
             : base::StrCat({card_name, u"  ", obfuscated_last_four});
}

std::u16string CreditCard::CardNameForAutofillDisplay(
    const std::u16string& customized_nickname) const {
  if (HasNonEmptyValidNickname() || !customized_nickname.empty()) {
    return customized_nickname.empty() ? nickname_ : customized_nickname;
  }
  if (base::FeatureList::IsEnabled(features::kAutofillEnableCardProductName) &&
      !product_description_.empty()) {
    return product_description_;
  }
  return NetworkForDisplay();
}

#if BUILDFLAG(IS_ANDROID)
std::u16string CreditCard::CardIdentifierStringForManualFilling() const {
  std::u16string obfuscated_number =
      ObfuscatedNumberWithVisibleLastFourDigits();
  if (record_type_ == VIRTUAL_CARD) {
    return l10n_util::GetStringUTF16(
               IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE) +
           u" " + obfuscated_number;
  }
  return obfuscated_number;
}
#endif  // BUILDFLAG(IS_ANDROID)

std::u16string CreditCard::CardIdentifierStringAndDescriptiveExpiration(
    const std::string& app_locale,
    std::u16string customized_nickname) const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_TWO_LINE_LABEL_FROM_NAME,
      CardIdentifierStringForAutofillDisplay(customized_nickname),
      GetInfo(AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale));
}

std::u16string CreditCard::DescriptiveExpiration(
    const std::string& app_locale) const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_TWO_LINE_LABEL_FROM_CARD_NUMBER,
      GetInfo(AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale));
}

std::u16string CreditCard::AbbreviatedExpirationDateForDisplay(
    bool with_prefix) const {
  std::u16string month = Expiration2DigitMonthAsString();
  std::u16string year = Expiration2DigitYearAsString();
  if (month.empty() || year.empty())
    return std::u16string();

  return l10n_util::GetStringFUTF16(
      with_prefix ? IDS_AUTOFILL_CREDIT_CARD_EXPIRATION_DATE_ABBR
                  : IDS_AUTOFILL_CREDIT_CARD_EXPIRATION_DATE_ABBR_V2,
      month, year);
}

std::u16string CreditCard::ExpirationDateForDisplay() const {
  std::u16string formatted_date(Expiration2DigitMonthAsString());
  formatted_date.append(u"/");
  formatted_date.append(Expiration4DigitYearAsString());
  return formatted_date;
}

std::u16string CreditCard::Expiration2DigitMonthAsString() const {
  return data_util::Expiration2DigitMonthAsString(expiration_month_);
}

std::u16string CreditCard::Expiration2DigitYearAsString() const {
  return data_util::Expiration2DigitYearAsString(expiration_year_);
}

std::u16string CreditCard::Expiration4DigitYearAsString() const {
  return data_util::Expiration4DigitYearAsString(expiration_year_);
}

bool CreditCard::HasNameOnCard() const {
  return !name_on_card_.empty();
}

bool CreditCard::HasNonEmptyValidNickname() const {
  // Valid nickname must not be empty.
  if (nickname_.empty())
    return false;

  return CreditCard::IsNicknameValid(nickname_);
}

std::u16string CreditCard::NicknameAndLastFourDigitsForTesting() const {
  return NicknameAndLastFourDigits();
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

std::u16string CreditCard::GetInfoImpl(const AutofillType& type,
                                       const std::string& app_locale) const {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == CREDIT_CARD_NUMBER) {
    // Web pages should never actually be filled by a masked server card,
    // but this function is used at the preview stage.
    if (record_type() == MASKED_SERVER_CARD)
      return NetworkAndLastFourDigits();

    return StripSeparators(number_);
  }

  return GetRawInfo(storable_type);
}

bool CreditCard::SetInfoWithVerificationStatusImpl(
    const AutofillType& type,
    const std::u16string& value,
    const std::string& app_locale,
    VerificationStatus status) {
  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == CREDIT_CARD_EXP_MONTH)
    return SetExpirationMonthFromString(value, app_locale);

  if (storable_type == CREDIT_CARD_NUMBER) {
    SetRawInfoWithVerificationStatus(storable_type, StripSeparators(value),
                                     status);
  } else {
    SetRawInfoWithVerificationStatus(storable_type, value, status);
  }
  return true;
}

std::u16string CreditCard::NetworkForFill() const {
  return ::autofill::NetworkForFill(network_);
}

std::u16string CreditCard::NicknameAndLastFourDigits(
    std::u16string customized_nickname,
    int obfuscation_length) const {
  // Should call HasNonEmptyValidNickname() to check valid nickname before
  // calling this.
  DCHECK(HasNonEmptyValidNickname() || !customized_nickname.empty());
  const std::u16string digits = LastFourDigits();
  // If digits are empty, return nickname.
  if (digits.empty())
    return customized_nickname.empty() ? nickname_ : customized_nickname;

  return (customized_nickname.empty() ? nickname_ : customized_nickname) +
         u"  " +
         internal::GetObfuscatedStringForCardDigits(digits, obfuscation_length);
}

void CreditCard::SetNumber(const std::u16string& number) {
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

bool CreditCard::IsExpired(const base::Time& current_time) const {
  return !IsValidCreditCardExpirationDate(expiration_year_, expiration_month_,
                                          current_time);
}

bool CreditCard::masked() const {
  return record_type() == CreditCard::MASKED_SERVER_CARD ||
         record_type() == CreditCard::VIRTUAL_CARD;
}

bool CreditCard::ShouldUpdateExpiration() const {
  return IsExpired(AutofillClock::Now());
}

bool CreditCard::IsCompleteValidCard() const {
  return !IsExpired(AutofillClock::Now()) && HasNameOnCard() &&
         (masked() || HasValidCardNumber());
}

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card) {
  return os << base::UTF16ToUTF8(credit_card.Label()) << " "
            << (credit_card.record_type() == CreditCard::LOCAL_CARD
                    ? credit_card.guid()
                    : base::HexEncode(credit_card.server_id().data(),
                                      credit_card.server_id().size()))
            << " " << credit_card.origin() << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL))
            << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_TYPE))
            << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_NUMBER))
            << " "
            << base::UTF16ToUTF8(credit_card.GetRawInfo(CREDIT_CARD_EXP_MONTH))
            << " "
            << base::UTF16ToUTF8(
                   credit_card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR))
            << " " << credit_card.bank_name() << " "
            << " " << credit_card.record_type() << " "
            << credit_card.use_count() << " " << credit_card.use_date() << " "
            << credit_card.billing_address_id() << " " << credit_card.nickname()
            << " " << credit_card.card_issuer() << " "
            << " " << credit_card.issuer_id() << " "
            << credit_card.instrument_id() << " "
            << credit_card.virtual_card_enrollment_state() << " "
            << credit_card.card_art_url().spec() << " "
            << base::UTF16ToUTF8(credit_card.product_description());
}

void CreditCard::SetNameOnCardFromSeparateParts() {
  DCHECK(!temp_card_first_name_.empty() && !temp_card_last_name_.empty());

  std::u16string new_name_on_card =
      temp_card_first_name_ + u" " + temp_card_last_name_;

  DCHECK(name_on_card_.empty() || name_on_card_ == new_name_on_card);

  name_on_card_ = new_name_on_card;
}

const char kAmericanExpressCard[] = "americanExpressCC";
const char kDinersCard[] = "dinersCC";
const char kDiscoverCard[] = "discoverCC";
const char kEloCard[] = "eloCC";
const char kGenericCard[] = "genericCC";
const char kJCBCard[] = "jcbCC";
const char kMasterCard[] = "masterCardCC";
const char kMirCard[] = "mirCC";
const char kTroyCard[] = "troyCC";
const char kUnionPay[] = "unionPayCC";
const char kVisaCard[] = "visaCC";

}  // namespace autofill
