// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/credit_card_field.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_filler.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Credit card numbers are at most 19 digits in length.
// [Ref: http://en.wikipedia.org/wiki/Bank_card_number]
constexpr size_t kMaxValidCardNumberSize = 19;

// Returns true if a field that has |max_length| can fit the data for a field of
// |type|.
bool FieldCanFitDataForFieldType(int max_length, ServerFieldType type) {
  if (max_length == 0)
    return true;

  switch (type) {
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      // A date with a 2 digit year can fit in a minimum of 4 chars (MMYY)
      static constexpr int kMinimum2YearCcExpLength = 4;
      return max_length >= kMinimum2YearCcExpLength;
    }
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      // A date with a 4 digit year can fit in a minimum of 6 chars (MMYYYY)
      static constexpr int kMinimum4YearCcExpLength = 6;
      return max_length >= kMinimum4YearCcExpLength;
    }
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

// static
std::unique_ptr<FormField> CreditCardField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  auto credit_card_field = std::make_unique<CreditCardField>(log_manager);
  size_t saved_cursor = scanner->SaveCursor();
  int nb_unknown_fields = 0;
  bool cardholder_name_match_has_low_confidence = false;

  base::span<const MatchPatternRef> name_on_card_patterns =
      GetMatchPatterns("NAME_ON_CARD", page_language, pattern_source);

  base::span<const MatchPatternRef> name_on_card_contextual_patterns =
      GetMatchPatterns("NAME_ON_CARD_CONTEXTUAL", page_language,
                       pattern_source);

  base::span<const MatchPatternRef> last_name_patterns =
      GetMatchPatterns("LAST_NAME", page_language, pattern_source);

  base::span<const MatchPatternRef> cvc_patterns = GetMatchPatterns(
      CREDIT_CARD_VERIFICATION_CODE, page_language, pattern_source);

  // Credit card fields can appear in many different orders.
  // We loop until no more credit card related fields are found, see |break| at
  // the bottom of the loop.
  for (int fields = 0; !scanner->IsEnd(); ++fields) {
    // Ignore gift card fields.
    if (IsGiftCardField(scanner, log_manager, page_language, pattern_source))
      break;

    if (!credit_card_field->cardholder_) {
      if (ParseField(scanner, kNameOnCardRe, name_on_card_patterns,
                     &credit_card_field->cardholder_,
                     {log_manager, "kNameOnCardRe"})) {
        continue;
      }

      // Sometimes the cardholder field is just labeled "name". Unfortunately
      // this is a dangerously generic word to search for, since it will often
      // match a name (not cardholder name) field before or after credit card
      // fields. So we search for "name" only when we've already parsed at
      // least one other credit card field and haven't yet parsed the
      // expiration date (which usually appears at the end).

      if (fields > 0 && !credit_card_field->expiration_month_ &&
          ParseField(scanner, kNameOnCardContextualRe,
                     name_on_card_contextual_patterns,
                     &credit_card_field->cardholder_,
                     {log_manager, "kNameOnCardContextualRe"})) {
        cardholder_name_match_has_low_confidence = true;
        continue;
      }
    } else if (!credit_card_field->cardholder_last_) {
      // Search for a last name. Since this is a dangerously generic search, we
      // execute it only after we have found a valid credit card (first) name
      // and haven't yet parsed the expiration date (which usually appears at
      // the end).
      if (!credit_card_field->expiration_month_ &&
          ParseField(scanner, kLastNameRe, last_name_patterns,
                     &credit_card_field->cardholder_last_,
                     {log_manager, "kLastNameRe"})) {
        continue;
      }
    }

    // Check for a credit card type (Visa, Mastercard, etc.) field.
    // All CC type fields encountered so far have been of type select.
    if (!credit_card_field->type_ && LikelyCardTypeSelectField(scanner)) {
      credit_card_field->type_ = scanner->Cursor();
      scanner->Advance();
      nb_unknown_fields = 0;
      continue;
    }

    // We look for a card security code before we look for a credit card number
    // and match the general term "number". The security code has a plethora of
    // names; we've seen "verification #", "verification number", "card
    // identification number", and others listed in the regex pattern used
    // below.
    // Note: Some sites use type="tel" or type="number" for numerical inputs.
    // They also sometimes use type="password" for sensitive types.
    const auto kMatchNumTelAndPwd =
        kDefaultMatchParamsWith<MatchFieldType::kNumber,
                                MatchFieldType::kTelephone,
                                MatchFieldType::kPassword>;

    if (!credit_card_field->verification_ &&
        ParseFieldSpecifics(scanner, kCardCvcRe, kMatchNumTelAndPwd,
                            cvc_patterns, &credit_card_field->verification_,
                            {log_manager, "kCardCvcRe"})) {
      // A couple of sites have multiple verification codes right after another.
      // Allow the classification of these codes one by one.
      AutofillField* const saved_cvv = credit_card_field->verification_;

      // Check if the verification code is the first detected field in the newly
      // started card.
      if (credit_card_field->numbers_.empty() &&
          !credit_card_field->HasExpiration() &&
          !credit_card_field->cardholder_ && scanner->SaveCursor() > 1) {
        // Check if the previous field was a verification code.
        scanner->RewindTo(scanner->SaveCursor() - 2);

        if (ParseFieldSpecifics(scanner, kCardCvcRe, kMatchNumTelAndPwd,
                                cvc_patterns, &credit_card_field->verification_,
                                {log_manager, "kCardCvcRe"})) {
          // Reset the current cvv (The verification parse overwrote it).
          credit_card_field->verification_ = saved_cvv;
          // Put the scanner back to the field right after the current cvv.
          scanner->Advance();
          return std::move(credit_card_field);
        } else {
          // Chances that verification field is the first of a card are really
          // low.
          scanner->Advance();
          credit_card_field->verification_ = nullptr;
        }
      } else {
        nb_unknown_fields = 0;
        continue;
      }
    }

    // TODO(crbug.com/591816): Make sure parsing cc-numbers of type password
    // doesn't have bad side effects.
    AutofillField* current_number_field;
    base::span<const MatchPatternRef> patterns =
        GetMatchPatterns(CREDIT_CARD_NUMBER, page_language, pattern_source);
    if (ParseFieldSpecifics(scanner, kCardNumberRe, kMatchNumTelAndPwd,
                            patterns, &current_number_field,
                            {log_manager, "kCardNumberRe"})) {
      // Avoid autofilling any credit card number field having very low or high
      // |start_index| on the HTML form.
      size_t start_index = 0;
      if (!credit_card_field->numbers_.empty()) {
        size_t last_number_field_size =
            credit_card_field->numbers_.back()->credit_card_number_offset() +
            credit_card_field->numbers_.back()->max_length;

        // Distinguish between
        //   (a) one card split across multiple fields
        //   (b) multiple fields for multiple cards
        // Treat this field as a part of the same card as the last field, except
        // when doing so would cause overflow.
        if (last_number_field_size < kMaxValidCardNumberSize)
          start_index = last_number_field_size;
      }

      current_number_field->set_credit_card_number_offset(start_index);
      credit_card_field->numbers_.push_back(current_number_field);
      nb_unknown_fields = 0;
      continue;
    }

    if (credit_card_field->ParseExpirationDate(scanner, log_manager,
                                               page_language, pattern_source)) {
      nb_unknown_fields = 0;
      continue;
    }

    if (credit_card_field->expiration_month_ &&
        !credit_card_field->expiration_year_ &&
        !credit_card_field->expiration_date_) {
      // Parsed a month but couldn't parse a year; give up.
      scanner->RewindTo(saved_cursor);
      return nullptr;
    }

    nb_unknown_fields++;

    // Since cc#/verification and expiration are inter-dependent for the final
    // detection decision, we allow for 4 UNKONWN fields in between.
    // We can't allow for a lot of unknown fields, because the name on address
    // sections may sometimes be mistakenly detected as cardholder name.
    if ((credit_card_field->verification_ ||
         !credit_card_field->numbers_.empty() ||
         credit_card_field->HasExpiration()) &&
        (!credit_card_field->verification_ ||
         credit_card_field->numbers_.empty() ||
         !credit_card_field->HasExpiration()) &&
        nb_unknown_fields < 4) {
      scanner->Advance();
      fields--;  // We continue searching in the same credit card section, but
                 // no more field is identified.
      continue;
    }
    break;
  }

  // Some pages have a billing address field after the cardholder name field.
  // For that case, allow only just the cardholder name field.  The remaining
  // CC fields will be picked up in a following CreditCardField.
  if (credit_card_field->cardholder_) {
    // If we got the cardholder name with a dangerous check, require at least a
    // card number and one of expiration or verification fields.
    if (!cardholder_name_match_has_low_confidence ||
        (!credit_card_field->numbers_.empty() &&
         (credit_card_field->verification_ ||
          credit_card_field->HasExpiration()))) {
      return std::move(credit_card_field);
    }
  }

  // On some pages, the user selects a card type using radio buttons
  // (e.g. test page Apple Store Billing.html).  We can't handle that yet,
  // so we treat the card type as optional for now.
  // The existence of a number or cvc in combination with expiration date is
  // a strong enough signal that this is a credit card.  It is possible that
  // the number and name were parsed in a separate part of the form.  So if
  // the cvc and date were found independently they are returned.
  const bool has_cc_number_or_verification =
      (credit_card_field->verification_ ||
       !credit_card_field->numbers_.empty());
  if (has_cc_number_or_verification && credit_card_field->HasExpiration())
    return std::move(credit_card_field);

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

// static
bool CreditCardField::LikelyCardMonthSelectField(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return false;

  AutofillField* field = scanner->Cursor();
  if (!MatchesFormControlType(
          field->form_control_type,
          {MatchFieldType::kSelect, MatchFieldType::kSearch})) {
    return false;
  }

  if (field->options.size() < 12 || field->options.size() > 13)
    return false;

  auto matches_december = [](const SelectOption& option) {
    static constexpr char16_t kNumericalDecemberRe[] = u"12";
    // Maybe we should do more here? E.g., look for (translated) "December".
    return MatchesRegex<kNumericalDecemberRe>(option.value) ||
           MatchesRegex<kNumericalDecemberRe>(option.content);
  };
  auto matches_year = [](const SelectOption& option) {
    static constexpr char16_t kNumericalYearRe[] = u"[1-9][0-9][0-9][0-9]";
    return MatchesRegex<kNumericalYearRe>(option.value) ||
           MatchesRegex<kNumericalYearRe>(option.content);
  };
  // If in doubt, return false.
  return matches_december(field->options.back()) &&
         !base::ranges::any_of(field->options, matches_year);
}

// static
bool CreditCardField::LikelyCardYearSelectField(
    AutofillScanner* scanner,
    LogManager* log_manager,
    const LanguageCode& page_language,
    PatternSource pattern_source) {
  if (scanner->IsEnd())
    return false;

  AutofillField* field = scanner->Cursor();
  if (!MatchesFormControlType(
          field->form_control_type,
          {MatchFieldType::kSelect, MatchFieldType::kSearch})) {
    return false;
  }

  // Filter out days - elements for date entries would have
  // numbers 1 to 9 as well in them, which we can filter on.
  auto matches_single_digit_date = [](const SelectOption& option) {
    static constexpr char16_t kSingleDigitDateRe[] = u"\\b[1-9]\\b";
    return MatchesRegex<kSingleDigitDateRe>(option.content);
  };
  if (base::ranges::any_of(field->options, matches_single_digit_date))
    return false;

  // Another way to eliminate days - filter out 'day' fields.
  base::span<const MatchPatternRef> day_patterns =
      GetMatchPatterns("DAY", page_language, pattern_source);
  if (FormField::ParseFieldSpecifics(
          scanner, kDayRe, kDefaultMatchParamsWith<MatchFieldType::kSelect>,
          day_patterns, nullptr, {log_manager, "kDayRe"})) {
    return false;
  }

  // Filter out birth years - a website would not offer 1999 as a credit card
  // expiration year, but show it in the context of a birth year selector.
  auto matches_birth_year = [](const SelectOption& option) {
    static constexpr char16_t kBirthYearRe[] = u"(1999|99)";
    return MatchesRegex<kBirthYearRe>(option.content);
  };
  if (base::ranges::any_of(field->options, matches_birth_year))
    return false;

  // Test if three consecutive items in `field->options` mention three
  // consecutive year dates.
  const base::Time time_now = AutofillClock::Now();
  base::Time::Exploded time_exploded;
  time_now.UTCExplode(&time_exploded);

  const int kYearsToMatch = 3;
  std::vector<std::u16string> years_to_check_2_digit;
  for (int year = time_exploded.year; year < time_exploded.year + kYearsToMatch;
       ++year) {
    years_to_check_2_digit.push_back(base::NumberToString16(year).substr(2));
  }

  auto OptionsContain = [&](const std::vector<std::u16string>& year_needles,
                            const auto& option_projection) {
    auto is_substring = [](base::StringPiece16 option,
                           base::StringPiece16 year_needle) {
      return option.find(year_needle) != base::StringPiece16::npos;
    };
    return base::ranges::search(field->options, year_needles, is_substring,
                                option_projection) != field->options.end();
  };
  return OptionsContain(years_to_check_2_digit, &SelectOption::value) ||
         OptionsContain(years_to_check_2_digit, &SelectOption::content);
}

// static
bool CreditCardField::LikelyCardTypeSelectField(AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return false;

  AutofillField* field = scanner->Cursor();

  if (!MatchesFormControlType(
          field->form_control_type,
          {MatchFieldType::kSelect, MatchFieldType::kSearch}))
    return false;

  // We set |ignore_whitespace| to true on these calls because this is actually
  // a pretty common mistake; e.g., "Master card" instead of "Mastercard".
  bool isSelect = (FieldFiller::FindShortestSubstringMatchInSelect(
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA), true,
                       field) >= 0) ||
                  (FieldFiller::FindShortestSubstringMatchInSelect(
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD),
                       true, field) >= 0);
  return isSelect;
}

// static
bool CreditCardField::IsGiftCardField(AutofillScanner* scanner,
                                      LogManager* log_manager,
                                      const LanguageCode& page_language,
                                      PatternSource pattern_source) {
  if (scanner->IsEnd())
    return false;

  // kMatchFieldType should subsume kMatchNumTelAndPwd used for
  // CREDIT_CARD_NUMBER matching. Otherwise, a gift card field may not match the
  // GIFT_CARD pattern but erroneously do match the CREDIT_CARD_NUMBER pattern.
  const auto kMatchFieldType = kDefaultMatchParamsWith<
      MatchFieldType::kNumber, MatchFieldType::kTelephone,
      MatchFieldType::kSearch, MatchFieldType::kPassword>;
  size_t saved_cursor = scanner->SaveCursor();

  base::span<const MatchPatternRef> debit_cards_patterns =
      GetMatchPatterns("DEBIT_CARD", page_language, pattern_source);

  base::span<const MatchPatternRef> debit_gift_card_patterns =
      GetMatchPatterns("DEBIT_GIFT_CARD", page_language, pattern_source);

  base::span<const MatchPatternRef> gift_card_patterns =
      GetMatchPatterns("GIFT_CARD", page_language, pattern_source);

  if (ParseFieldSpecifics(scanner, kDebitCardRe, kMatchFieldType,
                          debit_cards_patterns, nullptr,
                          {log_manager, "kDebitCardRe"})) {
    scanner->RewindTo(saved_cursor);
    return false;
  }
  if (ParseFieldSpecifics(scanner, kDebitGiftCardRe, kMatchFieldType,
                          debit_gift_card_patterns, nullptr,
                          {log_manager, "kDebitGiftCardRe"})) {
    scanner->RewindTo(saved_cursor);
    return false;
  }

  return ParseFieldSpecifics(scanner, kGiftCardRe, kMatchFieldType,
                             gift_card_patterns, nullptr,
                             {log_manager, "kGiftCardRe"});
}

CreditCardField::CreditCardField(LogManager* log_manager)
    : log_manager_(log_manager),
      cardholder_(nullptr),
      cardholder_last_(nullptr),
      type_(nullptr),
      verification_(nullptr),
      expiration_month_(nullptr),
      expiration_year_(nullptr),
      expiration_date_(nullptr),
      exp_year_type_(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {}

CreditCardField::~CreditCardField() {}

void CreditCardField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  for (auto* number : numbers_) {
    AddClassification(number, CREDIT_CARD_NUMBER,
                      kBaseCreditCardParserScore, field_candidates);
  }

  AddClassification(type_, CREDIT_CARD_TYPE, kBaseCreditCardParserScore,
                    field_candidates);
  AddClassification(verification_, CREDIT_CARD_VERIFICATION_CODE,
                    kBaseCreditCardParserScore, field_candidates);

  // If the heuristics detected first and last name in separate fields,
  // then ignore both fields. Putting them into separate fields is probably
  // wrong, because the credit card can also contain a middle name or middle
  // initial.
  if (cardholder_last_ == nullptr) {
    AddClassification(cardholder_, CREDIT_CARD_NAME_FULL,
                      kBaseCreditCardParserScore, field_candidates);
  } else {
    AddClassification(cardholder_, CREDIT_CARD_NAME_FIRST,
                      kBaseCreditCardParserScore, field_candidates);
    AddClassification(cardholder_last_, CREDIT_CARD_NAME_LAST,
                      kBaseCreditCardParserScore, field_candidates);
  }

  if (expiration_date_) {
    DCHECK(!expiration_month_);
    DCHECK(!expiration_year_);
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableExpirationDateImprovements)) {
      // We try to derive the expiration date from the max-length and label or
      // placeholder strings. If that's not possible, we fallback to the format
      // determined in `GetExpirationYearType()`.
      ServerFieldType fallback_type =
          GetExpirationYearType() == CREDIT_CARD_EXP_2_DIGIT_YEAR
              ? CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
              : CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
      ExpirationDateFormat format =
          CreditCardField::DetermineExpirationDateFormat(*expiration_date_,
                                                         fallback_type);
      AddClassification(expiration_date_,
                        format.digits_in_expiration_year == 2
                            ? CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
                            : CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                        kBaseCreditCardParserScore, field_candidates);
    } else {
      AddClassification(expiration_date_, GetExpirationYearType(),
                        kBaseCreditCardParserScore, field_candidates);
    }
  } else {
    AddClassification(expiration_month_, CREDIT_CARD_EXP_MONTH,
                      kBaseCreditCardParserScore, field_candidates);
    AddClassification(expiration_year_, GetExpirationYearType(),
                      kBaseCreditCardParserScore, field_candidates);
  }
}

bool CreditCardField::ParseExpirationDate(AutofillScanner* scanner,
                                          LogManager* log_manager,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source) {
  if (!expiration_date_ && base::EqualsCaseInsensitiveASCII(
                               scanner->Cursor()->form_control_type, "month")) {
    expiration_date_ = scanner->Cursor();
    expiration_month_ = nullptr;
    expiration_year_ = nullptr;
    scanner->Advance();
    return true;
  }

  if (expiration_month_ || expiration_date_)
    return false;

  // First try to parse split month/year expiration fields by looking for a
  // pair of select fields that look like month/year.
  if (ParseInAnyOrder(
          scanner, {{&expiration_month_,
                     base::BindRepeating(&LikelyCardMonthSelectField, scanner)},
                    {&expiration_year_,
                     base::BindRepeating(&LikelyCardYearSelectField, scanner,
                                         log_manager, page_language,
                                         pattern_source)}})) {
    return true;
  }

  // If that fails, do a general regex search.
  size_t month_year_saved_cursor = scanner->SaveCursor();
  const auto kMatchCCType =
      kDefaultMatchParamsWith<MatchFieldType::kNumber,
                              MatchFieldType::kTelephone,
                              MatchFieldType::kSelect, MatchFieldType::kSearch>;

  base::span<const MatchPatternRef> cc_exp_month_patterns =
      GetMatchPatterns(CREDIT_CARD_EXP_MONTH, page_language, pattern_source);

  base::span<const MatchPatternRef> cc_exp_year_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_YEAR", page_language, pattern_source);

  base::span<const MatchPatternRef> cc_exp_month_before_year_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_MONTH_BEFORE_YEAR", page_language,
                       pattern_source);

  base::span<const MatchPatternRef> cc_exp_year_after_month_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_YEAR_AFTER_MONTH", page_language,
                       pattern_source);

  if (ParseFieldSpecifics(scanner, kExpirationMonthRe, kMatchCCType,
                          cc_exp_month_patterns, &expiration_month_,
                          {log_manager_, "kExpirationMonthRe"}) &&
      ParseFieldSpecifics(scanner, kExpirationYearRe, kMatchCCType,
                          cc_exp_year_patterns, &expiration_year_,
                          {log_manager_, "kExpirationYearRe"})) {
    return true;
  }

  // If that fails, look for just MM and/or YY(YY) (or the Spanish/Portuguese
  // MM / AA(AA) version).
  scanner->RewindTo(month_year_saved_cursor);

  std::u16string year_pattern =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)
          ? u"^(yy|yyyy|aa|aaaa)$"
          : u"^(yy|yyyy)$";
  if (ParseFieldSpecifics(scanner, u"^mm$", kMatchCCType,
                          cc_exp_month_before_year_patterns, &expiration_month_,
                          {log_manager_, "^mm$"}) &&
      ParseFieldSpecifics(
          scanner, year_pattern, kMatchCCType, cc_exp_year_after_month_patterns,
          &expiration_year_,
          {log_manager_, base::UTF16ToUTF8(year_pattern).c_str()})) {
    return true;
  }

  // If that fails, try to parse a combined expiration field.
  // We allow <select> fields, because they're used e.g. on qvc.com.
  scanner->RewindTo(month_year_saved_cursor);

  // Bail out if the field cannot fit a 2-digit year expiration date.
  const int current_field_max_length = scanner->Cursor()->max_length;
  if (!FieldCanFitDataForFieldType(current_field_max_length,
                                   CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR))
    return false;

  // Try to look for a 2-digit year expiration date.
  base::span<const MatchPatternRef> cc_exp_2digit_year_patterns =
      GetMatchPatterns(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, page_language,
                       pattern_source);
  if (ParseFieldSpecifics(scanner, kExpirationDate2DigitYearRe, kMatchCCType,
                          cc_exp_2digit_year_patterns, &expiration_date_,
                          {log_manager_, "kExpirationDate2DigitYearRe"})) {
    exp_year_type_ = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    expiration_month_ = nullptr;
    return true;
  }

  // Try to look for a generic expiration date field. (2 or 4 digit year)
  base::span<const MatchPatternRef> cc_exp_date_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_DATE", page_language, pattern_source);
  if (ParseFieldSpecifics(scanner, kExpirationDateRe, kMatchCCType,
                          cc_exp_date_patterns, &expiration_date_,
                          {log_manager_, "kExpirationDateRe"})) {
    // If such a field exists, but it cannot fit a 4-digit year expiration
    // date, then the likely possibility is that it is a 2-digit year expiration
    // date.
    if (!FieldCanFitDataForFieldType(current_field_max_length,
                                     CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)) {
      exp_year_type_ = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    }
    expiration_month_ = nullptr;
    return true;
  }

  // Try to look for a 4-digit year expiration date.
  base::span<const MatchPatternRef> cc_exp_date_4_digit_year_patterns =
      GetMatchPatterns(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, page_language,
                       pattern_source);
  if (FieldCanFitDataForFieldType(current_field_max_length,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) &&
      ParseFieldSpecifics(scanner, kExpirationDate4DigitYearRe, kMatchCCType,
                          cc_exp_date_4_digit_year_patterns, &expiration_date_,
                          {log_manager_, "kExpirationDate4DigitYearRe"})) {
    expiration_month_ = nullptr;
    return true;
  }

  return false;
}

ServerFieldType CreditCardField::GetExpirationYearType() const {
  return (expiration_date_
              ? exp_year_type_
              : ((expiration_year_ && expiration_year_->max_length == 2)
                     ? CREDIT_CARD_EXP_2_DIGIT_YEAR
                     : CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

bool CreditCardField::HasExpiration() const {
  return expiration_date_ || (expiration_month_ && expiration_year_);
}

// static
CreditCardField::ExpirationDateFormat
CreditCardField::DetermineExpirationDateFormat(
    const AutofillField& field,
    ServerFieldType assumed_field_type) {
  CHECK(assumed_field_type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        assumed_field_type == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  static constexpr size_t kMonthLength = 2;  // 2 characters for a MM format.
  // Check whether we find one of the standard format descriptors like
  // "mm/yy", "mm/yyyy", "mm / yy", "mm-yyyy", ... in one of the human
  // readable labels. In that case, follow the specified pattern.
  std::vector<std::u16string> groups;
  bool matches = false;
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    static constexpr char16_t kFormatRegex[] =
        u"mm(\\s?[/-]?\\s?)?(y{2,4}|a{2,4})";
    //       ^^^^ opt white space
    //           ^^^^^ opt separator
    //                ^^^ opt white space
    //                       ^^^^^^^^^^^^^^ year
    matches = MatchesRegex<kFormatRegex>(field.placeholder, &groups) ||
              MatchesRegex<kFormatRegex>(field.label, &groups);
  } else {
    static constexpr char16_t kFormatRegEx[] = u"mm(\\s?[/-]?\\s?)?(y{2,4})";
    //                                              ^^^^ opt white space
    //                                                  ^^^^^ opt separator
    //                                                       ^^^ opt white space
    //                                                         year ^^^^^^^
    matches = MatchesRegex<kFormatRegEx>(field.placeholder, &groups) ||
              MatchesRegex<kFormatRegEx>(field.label, &groups);
  }
  // TODO(crbug/1326244): We should use language specific regex.
  if (matches) {
    const std::u16string& separator = groups[1];
    const std::u16string& year_format = groups[2];
    uint8_t year_length = year_format.length();
    uint8_t candidate_size = kMonthLength + separator.length() + year_length;
    if (field.max_length == 0 || candidate_size <= field.max_length) {
      return {separator, year_length};
    }
    // Try once more with a stripped version of the separator if the previous
    // version did not fit.
    base::StringPiece16 trimmed_separator =
        base::TrimWhitespace(groups[1], base::TRIM_ALL);
    candidate_size = kMonthLength + trimmed_separator.length() + year_length;
    if (field.max_length == 0 || candidate_size <= field.max_length) {
      return {std::u16string(trimmed_separator), year_length};
    }
  }

  switch (field.max_length) {
    case 1:
    case 2:
    case 3:
      // It is impossible to fill an expiration date in this size, so we
      // pick the shorted one.
      return {.separator = u"", .digits_in_expiration_year = 2};
    case 4:
      // Field likely expects MMYY
      return {.separator = u"", .digits_in_expiration_year = 2};
    case 5:
      // Field likely expects MM/YY
      return {.separator = u"/", .digits_in_expiration_year = 2};
    case 6:
      // Field likely expects MMYYYY
      return {.separator = u"", .digits_in_expiration_year = 4};
    case 7:
      // Field likely expects MM/YYYY
      return {.separator = u"/", .digits_in_expiration_year = 4};
    default:
      // Includes the case where max_length is not specified (0).
      return assumed_field_type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
                 ? ExpirationDateFormat{u"/", /*digits_in_expiration_year*/ 2}
                 : ExpirationDateFormat{u"/", /*digits_in_expiration_year*/ 4};
  }
}

}  // namespace autofill
