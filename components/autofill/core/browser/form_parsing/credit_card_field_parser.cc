// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/credit_card_field_parser.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/select_control_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

base::span<const MatchPatternRef> GetMatchPatterns(std::string_view name,
                                                   ParsingContext& context) {
  return GetMatchPatterns(name, context.page_language, context.pattern_file);
}

base::span<const MatchPatternRef> GetMatchPatterns(FieldType type,
                                                   ParsingContext& context) {
  return GetMatchPatterns(type, context.page_language, context.pattern_file);
}

// Returns true if a field that has |max_length| can fit the data for a field of
// |type|.
bool FieldCanFitDataForFieldType(uint64_t max_length, FieldType type) {
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
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

}  // namespace

// static
std::unique_ptr<FormFieldParser> CreditCardFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (scanner->IsEnd()) {
    return nullptr;
  }

  auto credit_card_field = std::make_unique<CreditCardFieldParser>();
  size_t saved_cursor = scanner->SaveCursor();
  int nb_unknown_fields = 0;
  bool cardholder_name_match_has_low_confidence = false;

  base::span<const MatchPatternRef> card_number_patterns =
      GetMatchPatterns(CREDIT_CARD_NUMBER, context);
  base::span<const MatchPatternRef> name_on_card_patterns =
      GetMatchPatterns("NAME_ON_CARD", context);
  base::span<const MatchPatternRef> name_on_card_contextual_patterns =
      GetMatchPatterns("NAME_ON_CARD_CONTEXTUAL", context);
  base::span<const MatchPatternRef> last_name_patterns =
      GetMatchPatterns("LAST_NAME", context);
  base::span<const MatchPatternRef> cvc_patterns =
      GetMatchPatterns(CREDIT_CARD_VERIFICATION_CODE, context);

  // Credit card fields can appear in many different orders.
  // We loop until no more credit card related fields are found, see |break| at
  // the bottom of the loop.
  for (int fields = 0; !scanner->IsEnd(); ++fields) {
    // Ignore gift card fields.
    if (IsGiftCardField(context, scanner)) {
      break;
    }

    if (!credit_card_field->cardholder_) {
      if (ParseField(context, scanner, name_on_card_patterns,
                     &credit_card_field->cardholder_, "NAME_ON_CARD")) {
        continue;
      }

      // Sometimes the cardholder field is just labeled "name". Unfortunately
      // this is a dangerously generic word to search for, since it will often
      // match a name (not cardholder name) field before or after credit card
      // fields. So we search for "name" only when we've already parsed at
      // least one other credit card field and haven't yet parsed the
      // expiration date (which usually appears at the end).

      if (fields > 0 && !credit_card_field->expiration_month_ &&
          ParseField(context, scanner, name_on_card_contextual_patterns,
                     &credit_card_field->cardholder_,
                     "NAME_ON_CARD_CONTEXTUAL")) {
        cardholder_name_match_has_low_confidence = true;
        continue;
      }
    } else if (!credit_card_field->cardholder_last_) {
      // Search for a last name. Since this is a dangerously generic search, we
      // execute it only after we have found a valid credit card (first) name
      // and haven't yet parsed the expiration date (which usually appears at
      // the end).
      if (!credit_card_field->expiration_month_ &&
          ParseField(context, scanner, last_name_patterns,
                     &credit_card_field->cardholder_last_, "LAST_NAME")) {
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

    if (!credit_card_field->verification_ &&
        ParseField(context, scanner, cvc_patterns,
                   &credit_card_field->verification_,
                   "CREDIT_CARD_VERIFICATION_CODE")) {
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

        if (ParseField(context, scanner, cvc_patterns,
                       &credit_card_field->verification_,
                       "CREDIT_CARD_VERIFICATION_CODE")) {
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

    // TODO(crbug.com/41242238): Make sure parsing cc-numbers of type password
    // doesn't have bad side effects.
    raw_ptr<AutofillField> current_number_field;
    if (ParseField(context, scanner, card_number_patterns,
                   &current_number_field, "CREDIT_CARD_NUMBER")) {
      credit_card_field->numbers_.push_back(current_number_field.get());
      nb_unknown_fields = 0;
      continue;
    }

    if (credit_card_field->ParseExpirationDate(context, scanner)) {
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
    //
    // While it does happen that a field separates the name from the CC number
    // (e.g. a tax payer ID), we still don't allow a field between name and
    // other fields because this generates false classifications for forms for
    // the structure ([no CC holder name present], CC number, CC exp date, CVC,
    // billing country, billing name, billing street). We have observed pretty
    // low n-counts of forms where the former and the latter problem occurred,
    // though.
    bool has_verification = credit_card_field->verification_;
    bool has_numbers = !credit_card_field->numbers_.empty();
    bool has_expiration = credit_card_field->HasExpiration();
    if ((has_verification || has_numbers || has_expiration) &&
        (!has_verification || !has_numbers || !has_expiration) &&
        nb_unknown_fields < 4) {
      scanner->Advance();
      fields--;  // We continue searching in the same credit card section, but
                 // no more field is identified.
      continue;
    }
    break;
  }

  bool has_verification = credit_card_field->verification_;
  bool has_numbers = !credit_card_field->numbers_.empty();
  bool has_expiration = credit_card_field->HasExpiration();
  // Some pages have a billing address field after the cardholder name field.
  // For that case, allow only just the cardholder name field.  The remaining
  // CC fields will be picked up in a following CreditCardFieldParser.
  if (credit_card_field->cardholder_) {
    // If we got the cardholder name with a dangerous check, require at least a
    // card number and one of expiration or verification fields.
    if (!cardholder_name_match_has_low_confidence ||
        (has_numbers && (has_verification || has_expiration))) {
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
  const bool has_cc_number_or_verification = (has_verification || has_numbers);
  if (has_cc_number_or_verification && credit_card_field->HasExpiration()) {
    return std::move(credit_card_field);
  }

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

// static
bool CreditCardFieldParser::LikelyCardMonthSelectField(
    AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return false;

  AutofillField* field = scanner->Cursor();
  if (!MatchesFormControlType(
          field->form_control_type(),
          {FormControlType::kSelectOne, FormControlType::kInputSearch})) {
    return false;
  }

  if (field->options().size() < 12 || field->options().size() > 13) {
    return false;
  }

  auto matches_december = [](const SelectOption& option) {
    static constexpr char16_t kNumericalDecemberRe[] = u"12";
    // Maybe we should do more here? E.g., look for (translated) "December".
    return MatchesRegex<kNumericalDecemberRe>(option.value) ||
           MatchesRegex<kNumericalDecemberRe>(option.text);
  };
  auto matches_year = [](const SelectOption& option) {
    static constexpr char16_t kNumericalYearRe[] = u"[1-9][0-9][0-9][0-9]";
    return MatchesRegex<kNumericalYearRe>(option.value) ||
           MatchesRegex<kNumericalYearRe>(option.text);
  };
  // If in doubt, return false.
  return matches_december(field->options().back()) &&
         !std::ranges::any_of(field->options(), matches_year);
}

// static
bool CreditCardFieldParser::LikelyCardYearSelectField(
    ParsingContext* context,
    AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return false;

  AutofillField* field = scanner->Cursor();
  if (!MatchesFormControlType(
          field->form_control_type(),
          {FormControlType::kSelectOne, FormControlType::kInputSearch})) {
    return false;
  }

  // Filter out days - elements for date entries would have
  // numbers 1 to 9 as well in them, which we can filter on.
  auto matches_single_digit_date = [](const SelectOption& option) {
    static constexpr char16_t kSingleDigitDateRe[] = u"\\b[1-9]\\b";
    return MatchesRegex<kSingleDigitDateRe>(option.text);
  };
  if (std::ranges::any_of(field->options(), matches_single_digit_date)) {
    return false;
  }

  // Another way to eliminate days - filter out 'day' fields.
  base::span<const MatchPatternRef> day_patterns =
      GetMatchPatterns("DAY", *context);
  if (FormFieldParser::ParseField(*context, scanner, day_patterns, nullptr,
                                  "DAY")) {
    return false;
  }

  // Filter out birth years - a website would not offer 1999 as a credit card
  // expiration year, but show it in the context of a birth year selector.
  auto matches_birth_year = [](const SelectOption& option) {
    static constexpr char16_t kBirthYearRe[] = u"(1999|99)";
    return MatchesRegex<kBirthYearRe>(option.text);
  };
  if (std::ranges::any_of(field->options(), matches_birth_year)) {
    return false;
  }

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
    // If the <option>s contain single-digits elements, this may lead to false
    // positives. Consider:
    // <option value="1">Afghanistan</option>
    // ...
    // <option value="23">Botswana</option>
    // While 23 is a valid expiration year, the selector is not a expiration
    // year selector. In case we find a single-digit entry, we reject this as
    // an expiration year selector.
    if (base::Contains(field->options(), u"2", option_projection)) {
      return false;
    }
    auto is_substring = [](std::u16string_view option,
                           std::u16string_view year_needle) {
      return option.find(year_needle) != std::u16string_view::npos;
    };
    return base::ranges::search(field->options(), year_needles, is_substring,
                                option_projection) != field->options().end();
  };
  return OptionsContain(years_to_check_2_digit, &SelectOption::value) ||
         OptionsContain(years_to_check_2_digit, &SelectOption::text);
}

// static
bool CreditCardFieldParser::LikelyCardTypeSelectField(
    AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return false;

  AutofillField* field = scanner->Cursor();

  if (!MatchesFormControlType(
          field->form_control_type(),
          {FormControlType::kSelectOne, FormControlType::kInputSearch})) {
    return false;
  }

  // We set |ignore_whitespace| to true on these calls because this is actually
  // a pretty common mistake; e.g., "Master card" instead of "Mastercard".
  bool isSelect = (FindShortestSubstringMatchInSelect(
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_VISA), true,
                       field->options()) >= 0) ||
                  (FindShortestSubstringMatchInSelect(
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_MASTERCARD),
                       true, field->options()) >= 0);
  return isSelect;
}

// static
bool CreditCardFieldParser::IsGiftCardField(ParsingContext& context,
                                            AutofillScanner* scanner) {
  if (scanner->IsEnd())
    return false;

  size_t saved_cursor = scanner->SaveCursor();

  base::span<const MatchPatternRef> debit_cards_patterns =
      GetMatchPatterns("DEBIT_CARD", context);
  base::span<const MatchPatternRef> debit_gift_card_patterns =
      GetMatchPatterns("DEBIT_GIFT_CARD", context);
  base::span<const MatchPatternRef> gift_card_patterns =
      GetMatchPatterns("GIFT_CARD", context);

  if (ParseField(context, scanner, debit_cards_patterns, nullptr,
                 "DEBIT_CARD")) {
    scanner->RewindTo(saved_cursor);
    return false;
  }
  if (ParseField(context, scanner, debit_gift_card_patterns, nullptr,
                 "DEBIT_GIFT_CARD")) {
    scanner->RewindTo(saved_cursor);
    return false;
  }

  return ParseField(context, scanner, gift_card_patterns, nullptr, "GIFT_CARD");
}

CreditCardFieldParser::CreditCardFieldParser()
    : cardholder_(nullptr),
      cardholder_last_(nullptr),
      type_(nullptr),
      verification_(nullptr),
      expiration_month_(nullptr),
      expiration_year_(nullptr),
      expiration_date_(nullptr),
      exp_year_type_(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {}

CreditCardFieldParser::~CreditCardFieldParser() = default;

void CreditCardFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  for (autofill::AutofillField* number : numbers_) {
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
      FieldType fallback_type =
          GetExpirationYearType() == CREDIT_CARD_EXP_2_DIGIT_YEAR
              ? CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
              : CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
      ExpirationDateFormat format =
          CreditCardFieldParser::DetermineExpirationDateFormat(
              *expiration_date_, /*fallback_type=*/fallback_type,
              /*server_hint=*/NO_SERVER_DATA,
              /*forced_field_type=*/NO_SERVER_DATA);
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

bool CreditCardFieldParser::ParseExpirationDate(ParsingContext& context,
                                                AutofillScanner* scanner) {
  if (!expiration_date_ &&
      scanner->Cursor()->form_control_type() == FormControlType::kInputMonth) {
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
          scanner,
          {{&expiration_month_,
            base::BindRepeating(&LikelyCardMonthSelectField, scanner)},
           {&expiration_year_, base::BindRepeating(&LikelyCardYearSelectField,
                                                   &context, scanner)}})) {
    return true;
  }

  // If that fails, do a general regex search.
  size_t month_year_saved_cursor = scanner->SaveCursor();

  base::span<const MatchPatternRef> cc_exp_month_patterns =
      GetMatchPatterns(CREDIT_CARD_EXP_MONTH, context);
  base::span<const MatchPatternRef> cc_exp_year_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_YEAR", context);
  base::span<const MatchPatternRef> cc_exp_month_before_year_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_MONTH_BEFORE_YEAR", context);
  base::span<const MatchPatternRef> cc_exp_year_after_month_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_YEAR_AFTER_MONTH", context);
  base::span<const MatchPatternRef>
      cc_exp_year_after_month_patterns_experimental = GetMatchPatterns(
          "CREDIT_CARD_EXP_YEAR_AFTER_MONTH_EXPERIMENTAL", context);

  if (ParseField(context, scanner, cc_exp_month_patterns, &expiration_month_,
                 "CREDIT_CARD_EXP_MONTH") &&
      ParseField(context, scanner, cc_exp_year_patterns, &expiration_year_,
                 "CREDIT_CARD_EXP_YEAR")) {
    return true;
  }

  // If that fails, look for just MM and/or YY(YY) (or the Spanish/Portuguese
  // MM / AA(AA) version).
  scanner->RewindTo(month_year_saved_cursor);

  std::u16string year_regex =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)
          ? u"^(yy|yyyy|aa|aaaa|jj|jjjj)$"
          : u"^(yy|yyyy)$";
  base::span<const MatchPatternRef> year_pattern =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)
          ? cc_exp_year_after_month_patterns_experimental
          : cc_exp_year_after_month_patterns;
  if (ParseField(context, scanner, cc_exp_month_before_year_patterns,
                 &expiration_month_, "^mm$") &&
      ParseField(context, scanner, year_pattern, &expiration_year_,
                 base::UTF16ToUTF8(year_regex).c_str())) {
    return true;
  }

  // If that fails, try to parse a combined expiration field.
  // We allow <select> fields, because they're used e.g. on qvc.com.
  scanner->RewindTo(month_year_saved_cursor);

  // Bail out if the field cannot fit a 2-digit year expiration date.
  const uint64_t current_field_max_length = scanner->Cursor()->max_length();
  if (!FieldCanFitDataForFieldType(current_field_max_length,
                                   CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR))
    return false;

  // Try to look for a 2-digit year expiration date.
  // If you add new languages, also update other places labeled with
  // [EXP_DATE_FORMAT].
  base::span<const MatchPatternRef> cc_exp_2digit_year_patterns =
      GetMatchPatterns(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, context);
  if (ParseField(context, scanner, cc_exp_2digit_year_patterns,
                 &expiration_date_, "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR")) {
    exp_year_type_ = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    expiration_month_ = nullptr;
    return true;
  }

  // Try to look for a generic expiration date field. (2 or 4 digit year)
  // If you add new languages, also update other places labeled with
  // [EXP_DATE_FORMAT].
  base::span<const MatchPatternRef> cc_exp_date_patterns =
      GetMatchPatterns("CREDIT_CARD_EXP_DATE", context);
  if (ParseField(context, scanner, cc_exp_date_patterns, &expiration_date_,
                 "CREDIT_CARD_EXP_DATE")) {
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
  // If you add new languages, also update other places labeled with
  // [EXP_DATE_FORMAT].
  base::span<const MatchPatternRef> cc_exp_date_4_digit_year_patterns =
      GetMatchPatterns(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, context);
  if (FieldCanFitDataForFieldType(current_field_max_length,
                                  CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) &&
      ParseField(context, scanner, cc_exp_date_4_digit_year_patterns,
                 &expiration_date_, "CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR")) {
    expiration_month_ = nullptr;
    return true;
  }

  return false;
}

// static
FieldType CreditCardFieldParser::DetermineExpirationYearType(
    const AutofillField& field,
    FieldType fallback_type,
    FieldType server_hint,
    FieldType forced_field_type) {
  // Forced server classifications always take priority if the field type
  // matches. Otherwise, the server override happens at a different spot.
  if (forced_field_type == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      forced_field_type == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    return forced_field_type;
  }

  // For text fields, look for placeholder patterns.
  if (field.IsTextInputElement()) {
    if (field.max_length() == 2) {
      return CREDIT_CARD_EXP_2_DIGIT_YEAR;
    }
    // If you add new languages, also update other places labeled with
    // [EXP_DATE_FORMAT].
    static constexpr char16_t kYYYYRegex[] = u"yyyy|aaaa|jjjj";
    if (MatchesRegex<kYYYYRegex>(field.placeholder(), nullptr) ||
        MatchesRegex<kYYYYRegex>(field.label(), nullptr)) {
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;
    }
    static constexpr char16_t kYYRegex[] = u"yy|aa|jj";
    if (MatchesRegex<kYYRegex>(field.placeholder(), nullptr) ||
        MatchesRegex<kYYRegex>(field.label(), nullptr)) {
      return CREDIT_CARD_EXP_2_DIGIT_YEAR;
    }
    if (field.max_length() == 4) {
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;
    }
  }

  // For select elements, look for today's year in the list of possible
  // expiration years and search for 4-digit and 2-digit representations.
  auto OptionsContain = [](const AutofillField& field,
                           const std::u16string& year_needle,
                           const auto& option_projection) {
    // If the <option>s contain single-digits elements, this may lead to false
    // positives. Consider:
    // <option value="1">Afghanistan</option>
    // ...
    // <option value="23">Botswana</option>
    // While 23 is a valid expiration year, the selector is not a expiration
    // year selector. In case we find a single-digit entry, we reject this as
    // an expiration year selector.
    if (base::Contains(field.options(), u"2", option_projection)) {
      return false;
    }
    auto is_substring = [&year_needle](std::u16string_view option) {
      return option.find(year_needle) != std::u16string_view::npos;
    };
    return std::ranges::any_of(field.options(), is_substring,
                               option_projection);
  };
  if (field.IsSelectElement()) {
    base::Time::Exploded time_exploded;
    AutofillClock::Now().UTCExplode(&time_exploded);
    std::u16string year_4_digits = base::NumberToString16(time_exploded.year);
    std::u16string year_2_digits = year_4_digits.substr(2);

    // Options are structured as <option value="$value">$content</option>.
    // Search in the $value first, because that's what's used for voting in
    // crowdsourcing and is more relevant.
    if (OptionsContain(field, year_4_digits, &SelectOption::value)) {
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;
    }
    if (OptionsContain(field, year_2_digits, &SelectOption::value)) {
      return CREDIT_CARD_EXP_2_DIGIT_YEAR;
    }
    // Fallback to content.
    if (OptionsContain(field, year_4_digits, &SelectOption::text)) {
      return CREDIT_CARD_EXP_4_DIGIT_YEAR;
    }
    if (OptionsContain(field, year_2_digits, &SelectOption::text)) {
      return CREDIT_CARD_EXP_2_DIGIT_YEAR;
    }
  }

  if (server_hint == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      server_hint == CREDIT_CARD_EXP_4_DIGIT_YEAR) {
    return server_hint;
  }
  return fallback_type;
}

FieldType CreditCardFieldParser::GetExpirationYearType() const {
  if (expiration_date_) {
    return exp_year_type_;
  }
  if (!expiration_year_) {
    return UNKNOWN_TYPE;
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    // The default for select or list elements does not really matter because
    // it's practically always chosen from the select options. The default for
    // text elements was chosen base on statistics from server side
    // classifications (go/iqwtu).
    // If you add new languages, also update other places labeled with
    // [EXP_DATE_FORMAT].
    return DetermineExpirationYearType(
        *expiration_year_,
        /*fallback_type=*/CREDIT_CARD_EXP_4_DIGIT_YEAR,
        /*server_hint=*/NO_SERVER_DATA,
        /*forced_field_type=*/NO_SERVER_DATA);
  }
  return expiration_year_->max_length() == 2 ? CREDIT_CARD_EXP_2_DIGIT_YEAR
                                             : CREDIT_CARD_EXP_4_DIGIT_YEAR;
}

bool CreditCardFieldParser::HasExpiration() const {
  return expiration_date_ || (expiration_month_ && expiration_year_);
}

// static
CreditCardFieldParser::ExpirationDateFormat
CreditCardFieldParser::DetermineExpirationDateFormat(
    const AutofillField& field,
    FieldType fallback_type,
    FieldType server_hint,
    FieldType forced_field_type) {
  CHECK(fallback_type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        fallback_type == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  static constexpr size_t kMonthLength = 2;  // 2 characters for a MM format.
  // Check whether we find one of the standard format descriptors like
  // "mm/yy", "mm/yyyy", "mm / yy", "mm-yyyy", ... in one of the human
  // readable labels. In that case, follow the specified pattern.
  std::vector<std::u16string> groups;
  bool matches = false;
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    // TODO(crbug.com/40225734): We should use a language specific regex.
    // If you add new languages, also update other places labeled with
    // [EXP_DATE_FORMAT].
    static constexpr char16_t kFormatRegex[] =
        u"mm(\\s?[/-]?\\s?)?(y{2,4}|a{2,4}|j{2,4})";
    //       ^^^^ opt white space
    //           ^^^^^ opt separator
    //                ^^^ opt white space
    //                       ^^^^^^^^^^^^^^ year
    matches = MatchesRegex<kFormatRegex>(field.placeholder(), &groups) ||
              MatchesRegex<kFormatRegex>(field.label(), &groups);
    // Support "--/--" and "--/----" as recognized placeholders.
    if (!matches) {
      static constexpr char16_t kFormatRegEx2[] =
          u"(?:--|__)(\\s?/\\s?)(-{2,4}|_{2,4})";
      matches = MatchesRegex<kFormatRegEx2>(field.placeholder(), &groups) ||
                MatchesRegex<kFormatRegEx2>(field.label(), &groups);
    }
  } else {
    static constexpr char16_t kFormatRegEx[] = u"mm(\\s?[/-]?\\s?)?(y{2,4})";
    //                                              ^^^^ opt white space
    //                                                  ^^^^^ opt separator
    //                                                       ^^^ opt white space
    //                                                         year ^^^^^^^
    matches = MatchesRegex<kFormatRegEx>(field.placeholder(), &groups) ||
              MatchesRegex<kFormatRegEx>(field.label(), &groups);
  }

  // Build a list of separator candidates from the regular expression sorted
  // by what we want to fill most.
  std::vector<std::u16string> separator_candidates;
  if (matches) {
    // First choice: The matching separator with padding whitespace.
    const std::u16string& separator = groups[1];
    separator_candidates.emplace_back(separator);

    // Fallback: The matching separator with padding whitespace trimmed.
    std::u16string_view trimmed_separator =
        base::TrimWhitespace(separator, base::TRIM_ALL);
    if (trimmed_separator != separator) {
      separator_candidates.emplace_back(trimmed_separator);
    }
  }
  // Add generic fallbacks.
  for (const char16_t* fallback : {u"/", u""}) {
    if (!base::Contains(separator_candidates, fallback)) {
      separator_candidates.emplace_back(fallback);
    }
  }

  // Build a list of lengths of the expiration year (this can only contain
  // entries for a length of 2 or 4 digits; or remain empty). The order of the
  // elements matters.
  // We may temporarily add 0 entries in case a specific parameter does not
  // have an indication for the format to use. This simplifies the code.
  constexpr uint8_t kInvalid = 0;
  auto type_length = [](FieldType type) -> uint8_t {
    return type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR   ? 2
           : type == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR ? 4
                                                       : kInvalid;
  };
  std::vector<uint8_t> year_length_candidates = {
      // First choice: Look at the forced field type (this is a server override
      // or the result of the entire classification chain).
      type_length(forced_field_type),
      // Fall back to a pattern found with the regex if the server does not have
      // an override. If the regex matched, groups[2] refers to the the the year
      // part (i.e. YY or YYYY in English strings).
      matches ? static_cast<uint8_t>(groups[2].length()) : kInvalid,
      // Finally, fall back to server hints if they are available.
      type_length(server_hint)};
  // Now erase all zeros that indicate that some of the three cases above did
  // not lead to a hint.
  std::erase(year_length_candidates, kInvalid);

  // If we don't have any concrete hints from the server or the matched date
  // pattern, we leave `year_length_candidates` empty (instead of always adding
  // a 4 and/or 2) as we have further heuristics based on `field.max_length`
  // that will be processed later.

  for (uint8_t year_length : year_length_candidates) {
    for (const std::u16string& separator : separator_candidates) {
      uint8_t candidate_size = kMonthLength + separator.length() + year_length;
      if (field.max_length() == 0 || candidate_size <= field.max_length()) {
        return {separator, year_length};
      }
    }
  }

  // Now use to the `field.max_length` attribute to guess an appropriate
  // format.
  switch (field.max_length()) {
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
      return fallback_type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
                 ? ExpirationDateFormat{u"/", /*digits_in_expiration_year*/ 2}
                 : ExpirationDateFormat{u"/", /*digits_in_expiration_year*/ 4};
  }
}

}  // namespace autofill
