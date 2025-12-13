// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payments/credit_card.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_model/payments/payments_metadata.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

constexpr char16_t kCreditCardObfuscationSymbol = '*';

constexpr int kMaxNicknameLength = 25;

// Suffix for GUID of a virtual card to differentiate it from it's corresponding
// masked server card.
constexpr char kVirtualCardIdentifierSuffix[] = "_vcn";

bool ShouldUseNewFopDisplay() {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  return false;
#else
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableNewFopDisplayDesktop);
#endif
}

std::u16string NetworkForFill(const std::string& network) {
  if (network == kAmericanExpressCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX);
  }
  if (network == kDinersCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DINERS);
  }
  if (network == kDiscoverCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_DISCOVER);
  }
  if (network == kEloCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_ELO);
  }
  if (network == kJCBCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_JCB);
  }
  if (network == kMasterCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD);
  }
  if (network == kMirCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MIR);
  }
  if (network == kTroyCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_TROY);
  }
  if (network == kUnionPay) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_UNION_PAY);
  }
  if (network == kVerveCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VERVE);
  }
  if (network == kVisaCard) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA);
  }

  // If you hit this DCHECK, the above list of cases needs to be updated to
  // include a new card.
  DCHECK_EQ(kGenericCard, network);
  return std::u16string();
}

// Returns the last four digits of the credit card `number` (fewer if there are
// not enough characters in `number`).
std::u16string GetLastFourDigits(std::u16string_view number) {
  static constexpr size_t kNumLastDigits = 4;

  std::u16string stripped = StripCardNumberSeparators(number);
  if (stripped.size() <= kNumLastDigits) {
    return stripped;
  }

  return stripped.substr(stripped.size() - kNumLastDigits, kNumLastDigits);
}

Suggestion::Icon ConvertCardNetworkIntoIcon(std::string_view network) {
  if (network == kAmericanExpressCard) {
    return Suggestion::Icon::kCardAmericanExpress;
  }
  if (network == kDinersCard) {
    return Suggestion::Icon::kCardDiners;
  }
  if (network == kDiscoverCard) {
    return Suggestion::Icon::kCardDiscover;
  }
  if (network == kEloCard) {
    return Suggestion::Icon::kCardElo;
  }
  if (network == kJCBCard) {
    return Suggestion::Icon::kCardJCB;
  }
  if (network == kMasterCard) {
    return Suggestion::Icon::kCardMasterCard;
  }
  if (network == kMirCard) {
    return Suggestion::Icon::kCardMir;
  }
  if (network == kTroyCard) {
    return Suggestion::Icon::kCardTroy;
  }
  if (network == kUnionPay) {
    return Suggestion::Icon::kCardUnionPay;
  }
  if (network == kVerveCard) {
    return Suggestion::Icon::kCardVerve;
  }
  if (network == kVisaCard) {
    return Suggestion::Icon::kCardVisa;
  }
  // If you hit this CHECK, the above list of cases needs to be updated to
  // include a new card.
  CHECK_EQ(network, kGenericCard);
  return Suggestion::Icon::kCardGeneric;
}

}  // namespace

// static
CreditCard CreditCard::CreateVirtualCard(const CreditCard& card) {
  // Virtual cards can be created only from masked server cards.
  DCHECK_EQ(card.record_type(), RecordType::kMaskedServerCard);
  CreditCard virtual_card = card;
  virtual_card.set_record_type(RecordType::kVirtualCard);
  return virtual_card;
}

// static
std::unique_ptr<CreditCard> CreditCard::CreateVirtualCardWithGuidSuffix(
    const CreditCard& card) {
  // Virtual cards can be created only from masked server cards.
  DCHECK_EQ(card.record_type(), RecordType::kMaskedServerCard);
  auto virtual_card = std::make_unique<CreditCard>(card);
  virtual_card->set_record_type(RecordType::kVirtualCard);
  // Add a suffix to the guid to help differentiate the virtual card from the
  // server card.
  virtual_card->set_guid(card.guid() + kVirtualCardIdentifierSuffix);
  return virtual_card;
}

// static
std::u16string CreditCard::GetObfuscatedStringForCardDigits(
    int obfuscation_length,
    const std::u16string& digits) {
  if (digits.empty()) {
    return {};
  }
  std::u16string obfuscated_string =
      CreditCard::GetMidlineEllipsisDots(obfuscation_length);
  obfuscated_string.append(digits);
  base::i18n::WrapStringWithLTRFormatting(&obfuscated_string);
  return obfuscated_string;
}

// static
std::string_view CreditCard::GetBenefitSourceStringFromEnum(
    BenefitSource benefit_source_enum) {
  switch (benefit_source_enum) {
    case BenefitSource::kSourceUnknown:
      return "";
    case BenefitSource::kSourceAmex:
      return kAmexCardBenefitSource;
    case BenefitSource::kSourceBmo:
      return kBmoCardBenefitSource;
    case BenefitSource::kSourceCurinos:
      return kCurinosCardBenefitSource;
  }
  NOTREACHED();
}

// static
CreditCard::BenefitSource CreditCard::GetEnumFromBenefitSourceString(
    std::string_view benefit_source_string) {
  if (benefit_source_string == kAmexCardBenefitSource) {
    return BenefitSource::kSourceAmex;
  }
  if (benefit_source_string == kBmoCardBenefitSource) {
    return BenefitSource::kSourceBmo;
  }
  if (benefit_source_string == kCurinosCardBenefitSource) {
    return BenefitSource::kSourceCurinos;
  }
  return BenefitSource::kSourceUnknown;
}

CreditCard::CreditCard(const std::string& guid, const std::string& origin)
    : guid_(guid),
      origin_(origin),
      record_type_(RecordType::kLocalCard),
      network_(kGenericCard),
      expiration_month_(0),
      expiration_year_(0),
      card_issuer_(Issuer::kIssuerUnknown),
      instrument_id_(0) {}

// TODO(crbug.com/40146355): Calling the CreditCard's default constructor
// initializes the `guid_`. This shouldn't happen for server cards, since they
// are not identified by guids. However, some of the server card logic relies
// by them for historical reasons.
CreditCard::CreditCard(RecordType type, const std::string& server_id)
    : CreditCard() {
  DCHECK(type == RecordType::kMaskedServerCard ||
         type == RecordType::kFullServerCard);
  record_type_ = type;
  server_id_ = server_id;
}

// TODO(crbug.com/40146355): See `server_id` constructor.
CreditCard::CreditCard(RecordType type, int64_t instrument_id) : CreditCard() {
  DCHECK(type == RecordType::kMaskedServerCard ||
         type == RecordType::kFullServerCard);
  record_type_ = type;
  instrument_id_ = instrument_id;
}

CreditCard::CreditCard()
    : CreditCard(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                 std::string()) {}

CreditCard::CreditCard(const CreditCard& credit_card) = default;
CreditCard::CreditCard(CreditCard&& credit_card) = default;
CreditCard& CreditCard::operator=(const CreditCard& credit_card) = default;
CreditCard& CreditCard::operator=(CreditCard&& credit_card) = default;

CreditCard::~CreditCard() = default;

// static
std::u16string CreditCard::NetworkForDisplay(const std::string& network) {
  if (kGenericCard == network) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_GENERIC);
  }
  if (kAmericanExpressCard == network) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX_SHORT);
  }

  return ::autofill::NetworkForFill(network);
}

// static
int CreditCard::IconResourceId(Suggestion::Icon icon) {
  switch (icon) {
    case Suggestion::Icon::kCardAmericanExpress:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_AMEX
                                      : IDR_AUTOFILL_METADATA_CC_AMEX_OLD;
    case Suggestion::Icon::kCardDiners:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_DINERS
                                      : IDR_AUTOFILL_METADATA_CC_DINERS_OLD;
    case Suggestion::Icon::kCardDiscover:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_DISCOVER
                                      : IDR_AUTOFILL_METADATA_CC_DISCOVER_OLD;
    case Suggestion::Icon::kCardElo:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_ELO
                                      : IDR_AUTOFILL_METADATA_CC_ELO_OLD;
    case Suggestion::Icon::kCardJCB:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_JCB
                                      : IDR_AUTOFILL_METADATA_CC_JCB_OLD;
    case Suggestion::Icon::kCardMasterCard:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_MASTERCARD
                                      : IDR_AUTOFILL_METADATA_CC_MASTERCARD_OLD;
    case Suggestion::Icon::kCardMir:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_MIR
                                      : IDR_AUTOFILL_METADATA_CC_MIR_OLD;
    case Suggestion::Icon::kCardTroy:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_TROY
                                      : IDR_AUTOFILL_METADATA_CC_TROY_OLD;
    case Suggestion::Icon::kCardUnionPay:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_UNIONPAY
                                      : IDR_AUTOFILL_METADATA_CC_UNIONPAY_OLD;
    case Suggestion::Icon::kCardVerve:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_VERVE
                                      : IDR_AUTOFILL_METADATA_CC_VERVE_OLD;
    case Suggestion::Icon::kCardVisa:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_VISA
                                      : IDR_AUTOFILL_METADATA_CC_VISA_OLD;
    case Suggestion::Icon::kCardGeneric:
      return ShouldUseNewFopDisplay() ? IDR_AUTOFILL_METADATA_CC_GENERIC
                                      : IDR_AUTOFILL_METADATA_CC_GENERIC_OLD;
    case Suggestion::Icon::kAccount:
    case Suggestion::Icon::kClear:
    case Suggestion::Icon::kCode:
    case Suggestion::Icon::kDelete:
    case Suggestion::Icon::kDevice:
    case Suggestion::Icon::kVehicle:
    case Suggestion::Icon::kWork:
    case Suggestion::Icon::kEdit:
    case Suggestion::Icon::kEmail:
    case Suggestion::Icon::kError:
    case Suggestion::Icon::kGlobe:
    case Suggestion::Icon::kGoogle:
    case Suggestion::Icon::kGoogleMonochrome:
    case Suggestion::Icon::kGooglePasswordManager:
    case Suggestion::Icon::kGooglePay:
    case Suggestion::Icon::kHome:
    case Suggestion::Icon::kIdCard:
    case Suggestion::Icon::kIban:
    case Suggestion::Icon::kKey:
    case Suggestion::Icon::kLocation:
    case Suggestion::Icon::kLoyalty:
    case Suggestion::Icon::kMagic:
    case Suggestion::Icon::kNoIcon:
    case Suggestion::Icon::kOfferTag:
    case Suggestion::Icon::kPenSpark:
    case Suggestion::Icon::kPersonCheck:
    case Suggestion::Icon::kPlusAddress:
    case Suggestion::Icon::kQuestionMark:
    case Suggestion::Icon::kRecoveryPassword:
    case Suggestion::Icon::kSaveAndFill:
    case Suggestion::Icon::kScanCreditCard:
    case Suggestion::Icon::kSettings:
    case Suggestion::Icon::kUndo:
    case Suggestion::Icon::kBnpl:
    case Suggestion::Icon::kGoogleWallet:
    case Suggestion::Icon::kGoogleWalletMonochrome:
    case Suggestion::Icon::kAndroidMessages:
    case Suggestion::Icon::kFlight:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
int CreditCard::IconResourceId(std::string_view icon_str) {
  return IconResourceId(ConvertCardNetworkIntoIcon(icon_str));
}

// static
bool CreditCard::IsNicknameValid(const std::u16string& nickname) {
  // Must not exceed max length.
  if (nickname.size() > kMaxNicknameLength) {
    return false;
  }

  // Must not contain newlines, tabs, or carriage returns.
  if (nickname.find('\n') != std::u16string::npos ||
      nickname.find('\r') != std::u16string::npos ||
      nickname.find('\t') != std::u16string::npos) {
    return false;
  }

  // Must not contain digits.
  for (char16_t c : nickname) {
    if (base::IsAsciiDigit(c)) {
      return false;
    }
  }

  return true;
}

// static
std::u16string CreditCard::GetMidlineEllipsisDots(size_t num_dots) {
  std::u16string dots;
  dots.reserve(sizeof(kMidlineEllipsisDot) * num_dots);

  for (size_t i = 0; i < num_dots; i++) {
    dots.append(kMidlineEllipsisDot);
  }
  return dots;
}

// static
std::u16string CreditCard::GetMidlineEllipsisPlainDots(size_t num_dots) {
  return std::u16string(num_dots, kMidlineEllipsisPlainDot);
}

// static
bool CreditCard::IsLocalCard(const CreditCard* card) {
  return card && card->record_type() == CreditCard::RecordType::kLocalCard;
}

void CreditCard::SetNetworkForMaskedCard(std::string_view network) {
  DCHECK_EQ(RecordType::kMaskedServerCard, record_type());
  network_ = std::string(network);
}

PaymentsMetadata CreditCard::GetMetadata() const {
  PaymentsMetadata metadata(usage_history_information_);
  metadata.id = (record_type_ == RecordType::kLocalCard ? guid() : server_id_);
  metadata.billing_address_id = billing_address_id_;
  return metadata;
}

double CreditCard::GetRankingScore(base::Time current_time) const {
  return usage_history_information_.GetRankingScore(current_time);
}

bool CreditCard::HasGreaterRankingThan(const CreditCard& other,
                                       base::Time comparison_time) const {
  const double score = GetRankingScore(comparison_time);
  const double other_score = other.GetRankingScore(comparison_time);
  return usage_history_information_.CompareRankingScores(
      score, other_score, other.usage_history_information_.use_date());
}

bool CreditCard::SetMetadata(const PaymentsMetadata& metadata) {
  // Make sure the ids matches.
  if (metadata.id !=
      (record_type_ == RecordType::kLocalCard ? guid() : server_id_)) {
    return false;
  }
  usage_history_information_.set_use_count(metadata.use_count);
  usage_history_information_.set_use_date(metadata.use_date);
  billing_address_id_ = metadata.billing_address_id;
  return true;
}

bool CreditCard::IsDeletable() const {
  return IsAutofillEntryWithUseDateDeletable(
             usage_history_information_.use_date()) &&
         IsExpired(AutofillClock::Now() - kDisusedDataModelDeletionTimeDelta);
}

std::u16string CreditCard::GetRawInfo(FieldType type) const {
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
      return name_on_card_;

    case CREDIT_CARD_NAME_FIRST:
      if (!temp_card_first_name_.empty()) {
        return temp_card_first_name_;
      }
      return data_util::SplitName(name_on_card_).given;

    case CREDIT_CARD_NAME_LAST:
      if (!temp_card_last_name_.empty()) {
        return temp_card_last_name_;
      }
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
      if (!month.empty() && !year.empty()) {
        return month + u"/" + year;
      }
      return std::u16string();
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      std::u16string month = Expiration2DigitMonthAsString();
      std::u16string year = Expiration4DigitYearAsString();
      if (!month.empty() && !year.empty()) {
        return month + u"/" + year;
      }
      return std::u16string();
    }

    case CREDIT_CARD_TYPE:
      return NetworkForFill();

    case CREDIT_CARD_NUMBER:
      return number_;

    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return cvc_;

    default:
      // ComputeDataPresentForArray will hit this repeatedly.
      return std::u16string();
  }
}

void CreditCard::SetRawInfoWithVerificationStatus(FieldType type,
                                                  std::u16string_view value,
                                                  VerificationStatus status) {
  DCHECK(FieldTypeGroupSet(
             {FieldTypeGroup::kCreditCard, FieldTypeGroup::kStandaloneCvcField})
             .contains(GroupTypeOfFieldType(type)));
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
      name_on_card_ = value;
      temp_card_first_name_ = u"";
      temp_card_last_name_ = u"";
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
      SetExpirationMonthFromString(value, {});
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
      if (value.size() > 0 && value[0] != kCreditCardObfuscationSymbol) {
        SetNumber(std::u16string(value));
      }
      break;
    }

    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      cvc_ = value;
      break;

    default:
      NOTREACHED() << "Attempting to set unknown info-type " << type;
  }
}

void CreditCard::GetMatchingTypes(std::u16string_view text,
                                  std::string_view app_locale,
                                  FieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  std::u16string card_number = GetInfo(CREDIT_CARD_NUMBER, app_locale);
  if (!card_number.empty()) {
    // We only have the last four digits for masked cards, so match against
    // that if |this| is a masked card.
    bool numbers_match = record_type_ == RecordType::kMaskedServerCard
                             ? GetLastFourDigits(text) == LastFourDigits()
                             : StripCardNumberSeparators(text) == card_number;
    if (numbers_match) {
      matching_types->insert(CREDIT_CARD_NUMBER);
    }
  }

  if (std::optional<int> parsed_month =
          data_util::ParseMonthFromString(text, app_locale)) {
    if (*parsed_month == expiration_month_) {
      matching_types->insert(CREDIT_CARD_EXP_MONTH);
    }
  }
}

void CreditCard::SetInfoForMonthInputType(const std::u16string& value) {
  static constexpr char16_t kDateRegex[] = u"^[0-9]{4}-[0-9]{1,2}$";
  // Check if |text| is "yyyy-mm" format first, and check normal month format.
  if (!MatchesRegex<kDateRegex>(value)) {
    return;
  }

  std::vector<std::u16string_view> year_month = base::SplitStringPiece(
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
  if (std::optional<int> parsed_month =
          data_util::GetExpirationMonth(expiration_month)) {
    expiration_month_ = *parsed_month;
  }
}

void CreditCard::SetExpirationYear(int expiration_year) {
  if (std::optional<int> parsed_year =
          data_util::GetExpirationYear(expiration_year)) {
    expiration_year_ = *parsed_year;
  }
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
  if (this->GetInfo(CREDIT_CARD_NUMBER, app_locale) !=
      imported_card.GetInfo(CREDIT_CARD_NUMBER, app_locale)) {
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
  if (!imported_card.name_on_card_.empty()) {
    name_on_card_ = imported_card.name_on_card_;
  }

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
  const FieldType types[] = {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                             CREDIT_CARD_EXP_4_DIGIT_YEAR};
  for (FieldType type : types) {
    int comparison = GetRawInfo(type).compare(credit_card.GetRawInfo(type));
    if (comparison != 0) {
      return comparison;
    }
  }

  if (!MatchingCardDetails(credit_card)) {
    return number().compare(credit_card.number());
  }

  int comparison = server_id_.compare(credit_card.server_id_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = billing_address_id_.compare(credit_card.billing_address_id_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = nickname_.compare(credit_card.nickname_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = product_description_.compare(credit_card.product_description_);
  if (comparison != 0) {
    return comparison;
  }

  comparison = cvc_.compare(credit_card.cvc_);
  if (comparison != 0) {
    return comparison;
  }

  if (static_cast<int>(card_issuer_) <
      static_cast<int>(credit_card.card_issuer_)) {
    return -1;
  }
  if (static_cast<int>(card_issuer_) >
      static_cast<int>(credit_card.card_issuer_)) {
    return 1;
  }

  comparison = issuer_id_.compare(credit_card.issuer_id_);
  if (comparison != 0) {
    return comparison;
  }

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
  if (comparison != 0) {
    return comparison;
  }

  comparison =
      product_terms_url_.spec().compare(credit_card.product_terms_url_.spec());
  if (comparison != 0) {
    return comparison;
  }

  comparison = benefit_source_.compare(credit_card.benefit_source_);
  if (comparison != 0) {
    return comparison;
  }

  // Do not distinguish masked server cards from full server cards as this is
  // not needed and not desired - we want to identify masked server card from
  // sync with the (potential) full server card stored locally.
  if (record_type_ == RecordType::kLocalCard &&
      credit_card.record_type_ != RecordType::kLocalCard) {
    return -1;
  }
  if (record_type_ != RecordType::kLocalCard &&
      credit_card.record_type_ == RecordType::kLocalCard) {
    return 1;
  }

  if (static_cast<int>(card_info_retrieval_enrollment_state_) <
      static_cast<int>(credit_card.card_info_retrieval_enrollment_state_)) {
    return -1;
  }
  if (static_cast<int>(card_info_retrieval_enrollment_state_) >
      static_cast<int>(credit_card.card_info_retrieval_enrollment_state_)) {
    return 1;
  }

  if (static_cast<int>(card_creation_source_) <
      static_cast<int>(credit_card.card_creation_source_)) {
    return -1;
  }
  if (static_cast<int>(card_creation_source_) >
      static_cast<int>(credit_card.card_creation_source_)) {
    return 1;
  }
  return 0;
}

bool CreditCard::IsLocalOrServerDuplicateOf(const CreditCard& other) const {
  if (record_type() == other.record_type()) {
    return false;
  }
  // If `this` or `other` is only a partial card, i.e. some fields are
  // missing, assume those fields match.
  bool name_on_card_differs =
      !name_on_card_.empty() && !other.name_on_card_.empty() &&
      !base::EqualsCaseInsensitiveASCII(name_on_card_, other.name_on_card_);
  bool expiration_month_differs = expiration_month_ != 0 &&
                                  other.expiration_month_ != 0 &&
                                  expiration_month_ != other.expiration_month_;
  bool expiration_year_differs = expiration_year_ != 0 &&
                                 other.expiration_year_ != 0 &&
                                 expiration_year_ != other.expiration_year_;
  bool billing_address_differs =
      !billing_address_id_.empty() && !other.billing_address_id_.empty() &&
      billing_address_id_ != other.billing_address_id_;

  if (name_on_card_differs || expiration_month_differs ||
      expiration_year_differs || billing_address_differs) {
    return false;
  }

  if (number_.empty() || other.number_.empty()) {
    return true;
  }

  return MatchingCardDetails(other);
}

bool CreditCard::MatchingCardDetails(const CreditCard& other) const {
  // Masked cards are considered to have the same number if their last four
  // digits match and if any expiration date information available for both
  // cards matches.
  if (record_type() == RecordType::kMaskedServerCard ||
      other.record_type() == RecordType::kMaskedServerCard) {
    bool last_four_digits_match = HasSameNumberAs(other);

    bool months_match = expiration_month() == other.expiration_month() ||
                        expiration_month() == 0 ||
                        other.expiration_month() == 0;

    bool years_match = expiration_year() == other.expiration_year() ||
                       expiration_year() == 0 || other.expiration_year() == 0;

    return last_four_digits_match && months_match && years_match;
  }

  return HasSameNumberAs(other);
}

bool CreditCard::HasSameNumberAs(const CreditCard& other) const {
  if (record_type() == CreditCard::RecordType::kMaskedServerCard ||
      other.record_type() == CreditCard::RecordType::kMaskedServerCard) {
    return LastFourDigits() == other.LastFourDigits();
  }

  return StripCardNumberSeparators(number_) ==
         StripCardNumberSeparators(other.number_);
}

bool CreditCard::HasSameExpirationDateAs(const CreditCard& other) const {
  return expiration_month() == other.expiration_month() &&
         expiration_year() == other.expiration_year();
}

bool CreditCard::operator==(const CreditCard& credit_card) const {
  return guid() == credit_card.guid() && origin() == credit_card.origin() &&
         record_type() == credit_card.record_type() &&
         Compare(credit_card) == 0;
}

bool CreditCard::IsVerified() const {
  return !origin_.empty() && !GURL(origin_).is_valid();
}

bool CreditCard::IsEmpty(const std::string& app_locale) const {
  FieldTypeSet types;
  GetNonEmptyTypes(app_locale, &types);
  return types.empty();
}

bool CreditCard::IsEnrolledInCardInfoRetrieval() const {
  return card_info_retrieval_enrollment_state() ==
             CardInfoRetrievalEnrollmentState::kRetrievalEnrolled &&
         record_type() == RecordType::kMaskedServerCard;
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

bool CreditCard::SetExpirationMonthFromString(std::u16string_view text,
                                              std::string_view app_locale) {
  if (std::optional<int> parsed_month =
          data_util::ParseMonthFromString(text, app_locale)) {
    expiration_month_ = *parsed_month;
    return true;
  }
  return false;
}

bool CreditCard::SetExpirationYearFromString(std::u16string_view text) {
  if (std::optional<int> parsed_year = data_util::ParseYearFromString(text)) {
    expiration_year_ = *parsed_year;
    return true;
  }
  return false;
}

void CreditCard::SetExpirationDateFromString(std::u16string_view text) {
  static constexpr char16_t kDateRegex[] =
      uR"(^\s*[0-9]{1,2}\s*[-/|]?\s*[0-9]{2,4}\s*$)";
  // Check that `text` fits the supported patterns: mmyy, mmyyyy, m-yy,
  // mm-yy, m-yyyy and mm-yyyy. Note that myy and myyyy matched by this pattern
  // but are not supported (ambiguous). Separators: -, / and |.
  if (!MatchesRegex<kDateRegex>(text)) {
    return;
  }

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
    if (HasNonEmptyValidNickname()) {
      return std::make_pair(nickname_, std::u16string());
    }

    return std::make_pair(name_on_card_, std::u16string());
  }

  if (ShouldUseNewFopDisplay()) {
    if (CardIdentifierForAutofillDisplay().has_value()) {
      return std::make_pair(CardIdentifierForAutofillDisplay().value(),
                            NetworkAndLastFourDigits(/*obfuscation_length=*/2));
    }
    return std::make_pair(NetworkAndLastFourDigits(/*obfuscation_length=*/2),
                          std::u16string());
  }

  return std::make_pair(CardNameAndLastFourDigits(), name_on_card_);
}

std::u16string CreditCard::Label() const {
  std::pair<std::u16string, std::u16string> pieces = LabelPieces();
  if (pieces.first.empty() || pieces.second.empty()) {
    return pieces.first + pieces.second;
  }

  return base::StrCat({pieces.first, u", ", pieces.second});
}

std::u16string CreditCard::LastFourDigits() const {
  return GetLastFourDigits(number_);
}

std::u16string CreditCard::FullDigitsForDisplay() const {
  return GetFormattedCardNumberForDisplay(number_);
}

std::u16string CreditCard::NetworkForDisplay() const {
  return CreditCard::NetworkForDisplay(network_);
}

std::u16string CreditCard::ObfuscatedNumberWithVisibleLastFourDigits(
    int obfuscation_length) const {
  return GetObfuscatedStringForCardDigits(obfuscation_length, LastFourDigits());
}

std::u16string
CreditCard::ObfuscatedNumberWithVisibleLastFourDigitsForSplitFields() const {
  // For split credit card number fields, use plain dots without spacing and no
  // LTR formatting. Only obfuscate 12 dots and append the last four digits of
  // the credit card number.
  return std::u16string(12, kMidlineEllipsisPlainDot) + LastFourDigits();
}

Suggestion::Icon CreditCard::CardIconForAutofillSuggestion() const {
  return ConvertCardNetworkIntoIcon(network_);
}

std::u16string CreditCard::NetworkAndLastFourDigits(
    int obfuscation_length) const {
  const std::u16string network = NetworkForDisplay();
  const std::u16string obfuscated_last_four =
      ObfuscatedNumberWithVisibleLastFourDigits(obfuscation_length);
  return base::StrCat(
      {network, (network.empty() || obfuscated_last_four.empty() ? u"" : u"  "),
       obfuscated_last_four});
}

std::u16string CreditCard::CardNameAndLastFourDigits(
    std::u16string customized_nickname,
    int obfuscation_length) const {
  const std::u16string card_name =
      CardNameForAutofillDisplay(customized_nickname);
  const std::u16string obfuscated_last_four =
      ObfuscatedNumberWithVisibleLastFourDigits(obfuscation_length);
  return base::StrCat(
      {card_name,
       (card_name.empty() || obfuscated_last_four.empty() ? u"" : u"  "),
       obfuscated_last_four});
}

std::u16string CreditCard::CardNameForAutofillDisplay(
    const std::u16string& customized_nickname) const {
  if (HasNonEmptyValidNickname() || !customized_nickname.empty()) {
    return customized_nickname.empty() ? nickname_ : customized_nickname;
  }
  if (!product_description_.empty()) {
    return product_description_;
  }
  return NetworkForDisplay();
}

#if BUILDFLAG(IS_ANDROID)
std::u16string CreditCard::CardIdentifierStringForManualFilling() const {
  std::u16string obfuscated_number =
      ObfuscatedNumberWithVisibleLastFourDigits();
  if (record_type_ == RecordType::kVirtualCard) {
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
      CardNameAndLastFourDigits(customized_nickname),
      GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale));
}

std::optional<std::u16string> CreditCard::CardIdentifierForAutofillDisplay(
    const std::u16string& customized_nickname) const {
  if (!customized_nickname.empty()) {
    return customized_nickname;
  }
  if (HasNonEmptyValidNickname()) {
    return nickname_;
  }
  if (!product_description_.empty()) {
    return product_description_;
  }
  return std::nullopt;
}

std::u16string CreditCard::DescriptiveExpiration(
    const std::string& app_locale) const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_TWO_LINE_LABEL_FROM_CARD_NUMBER,
      GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale));
}

std::u16string CreditCard::AbbreviatedExpirationDateForDisplay(
    bool with_prefix) const {
  std::u16string month = Expiration2DigitMonthAsString();
  std::u16string year = Expiration2DigitYearAsString();
  if (month.empty() || year.empty()) {
    return std::u16string();
  }

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
  if (nickname_.empty()) {
    return false;
  }

  return CreditCard::IsNicknameValid(nickname_);
}

std::u16string CreditCard::NicknameAndLastFourDigitsForTesting() const {
  return NicknameAndLastFourDigits();
}

bool CreditCard::HasRichCardArtImageFromMetadata() const {
  return card_art_url().is_valid() &&
         card_art_url().spec() != kCapitalOneLargeCardArtUrl &&
         card_art_url().spec() != kCapitalOneCardArtUrl;
}

FieldTypeSet CreditCard::GetSupportedTypes() const {
  static constexpr FieldTypeSet supported_types{
      CREDIT_CARD_NAME_FULL,
      CREDIT_CARD_NAME_FIRST,
      CREDIT_CARD_NAME_LAST,
      CREDIT_CARD_NUMBER,
      CREDIT_CARD_TYPE,
      CREDIT_CARD_EXP_MONTH,
      CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR};
  return supported_types;
}

std::u16string CreditCard::GetInfo(const AutofillType& autofill_type,
                                   std::string_view app_locale) const {
  const FieldType type = autofill_type.GetCreditCardType();
  if (type == CREDIT_CARD_NUMBER) {
    // Web pages should never actually be filled by a masked server card,
    // but this function is used at the preview stage.
    if (record_type() == RecordType::kMaskedServerCard) {
      return NetworkAndLastFourDigits();
    }
    return StripCardNumberSeparators(number_);
  }
  return GetRawInfo(type);
}

bool CreditCard::SetInfoWithVerificationStatus(const AutofillType& type,
                                               std::u16string_view value,
                                               std::string_view app_locale,
                                               VerificationStatus status) {
  const FieldType storable_type = type.GetCreditCardType();
  if (storable_type == CREDIT_CARD_EXP_MONTH) {
    return SetExpirationMonthFromString(value, app_locale);
  }

  if (storable_type == CREDIT_CARD_NUMBER) {
    SetRawInfoWithVerificationStatus(storable_type,
                                     StripCardNumberSeparators(value), status);
  } else {
    SetRawInfoWithVerificationStatus(storable_type, value, status);
  }
  return true;
}

VerificationStatus CreditCard::GetVerificationStatus(FieldType type) const {
  return VerificationStatus::kNoStatus;
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
  const std::u16string obfuscated_last_four =
      ObfuscatedNumberWithVisibleLastFourDigits(obfuscation_length);
  const std::u16string nickname =
      customized_nickname.empty() ? nickname_ : customized_nickname;
  return obfuscated_last_four.empty()
             ? nickname
             : base::StrCat({nickname, u"  ", obfuscated_last_four});
}

void CreditCard::SetNumber(std::u16string number) {
  number_ = std::move(number);

  // Set the type based on the card number, but only for full numbers, not
  // when we have masked cards from the server (last 4 digits).
  if (record_type_ != RecordType::kMaskedServerCard) {
    network_ = GetCardNetwork(StripCardNumberSeparators(number_));
  }
}

void CreditCard::RecordAndLogUse() {
  UMA_HISTOGRAM_COUNTS_1000(
      "Autofill.DaysSinceLastUse.CreditCard",
      (AutofillClock::Now() - usage_history_information_.use_date()).InDays());
  usage_history_information_.RecordUseDate(AutofillClock::Now());
  usage_history_information_.set_use_count(
      usage_history_information_.use_count() + 1);
}

bool CreditCard::IsExpired(base::Time current_time) const {
  return !IsValidCreditCardExpirationDate(expiration_year_, expiration_month_,
                                          current_time);
}

bool CreditCard::masked() const {
  return record_type() == CreditCard::RecordType::kMaskedServerCard ||
         record_type() == CreditCard::RecordType::kVirtualCard;
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
            << (credit_card.record_type() == CreditCard::RecordType::kLocalCard
                    ? credit_card.guid()
                    : base::HexEncode(credit_card.server_id()))
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
            << base::to_underlying(credit_card.record_type()) << " "
            << credit_card.usage_history().use_count() << " "
            << credit_card.usage_history().use_date() << " "
            << credit_card.billing_address_id() << " " << credit_card.nickname()
            << " "
            << static_cast<
                   typename std::underlying_type<CreditCard::Issuer>::type>(
                   credit_card.card_issuer())
            << " " << credit_card.issuer_id() << " "
            << credit_card.instrument_id() << " "
            << base::to_underlying(credit_card.virtual_card_enrollment_state())
            << " " << credit_card.card_art_url().spec() << " "
            << base::UTF16ToUTF8(credit_card.product_description()) << " "
            << credit_card.product_terms_url().spec() << " "
            << credit_card.benefit_source() << " " << credit_card.cvc() << " "
            << base::to_underlying(
                   credit_card.card_info_retrieval_enrollment_state())
            << " " << base::to_underlying(credit_card.card_creation_source());
}

void CreditCard::SetNameOnCardFromSeparateParts() {
  DCHECK(!temp_card_first_name_.empty() && !temp_card_last_name_.empty());
  name_on_card_ = temp_card_first_name_ + u" " + temp_card_last_name_;
  temp_card_first_name_ = u"";
  temp_card_last_name_ = u"";
}

UsageHistoryInformation& CreditCard::usage_history() {
  return usage_history_information_;
}
const UsageHistoryInformation& CreditCard::usage_history() const {
  return usage_history_information_;
}

}  // namespace autofill
