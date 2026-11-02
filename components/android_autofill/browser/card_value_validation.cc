// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/card_value_validation.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/origin.h"

namespace autofill {

namespace {

// Maximum length of a plausible cardholder name. Also stays well below
// `MatchesRegex()`'s 5000-character limit, above which `IsValidNameOnCard()`
// returns true even for strings with digits.
constexpr size_t kMaxNameLength = 100;

// The number of years after the current year that an expiration year may be.
constexpr int kMaxYearsInFuture = 20;

bool IsPlausibleMonth(std::u16string_view value) {
  // Check `size()` so that 3- or 4-digit CVCs with leading zeros (e.g. "007")
  // don't pass, and check `IsAsciiDigit` to reject "+".
  int month = 0;
  return value.size() <= 2 &&
         std::ranges::all_of(value, base::IsAsciiDigit<char16_t>) &&
         base::StringToInt(value, &month) && 1 <= month && month <= 12;
}

// Returns whether `value` is a 2-digit year or, if `allow_4_digits`, a 4-digit
// year within the next `kMaxYearsInFuture` years.
bool IsPlausibleYear(std::u16string_view value, bool allow_4_digits) {
  int year = 0;
  if (!std::ranges::all_of(value, base::IsAsciiDigit<char16_t>) ||
      !base::StringToInt(value, &year)) {
    return false;
  }
  if (value.size() != 2 && (value.size() != 4 || !allow_4_digits)) {
    return false;
  }
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  const int current_year = exploded.year;
  if (value.size() == 2) {
    // The first year with these last two digits that isn't in the past.
    year += current_year / 100 * 100;
    if (year < current_year) {
      year += 100;
    }
  }
  return current_year <= year && year <= current_year + kMaxYearsInFuture;
}

bool IsPlausibleExpirationDate(std::u16string_view value) {
  // Requiring a separator keeps 4-digit CVCs from passing as MMYY.
  const size_t separator = value.find_first_of(u"/-");
  if (separator == std::u16string_view::npos) {
    return false;
  }
  return IsPlausibleMonth(base::TrimWhitespace(value.substr(0, separator),
                                               base::TRIM_ALL)) &&
         IsPlausibleYear(
             base::TrimWhitespace(value.substr(separator + 1), base::TRIM_ALL),
             /*allow_4_digits=*/true);
}

// The parts of an expiration date. At most one field per part gets a type in
// each fill.
enum class ExpirationPart {
  kMonth,
  kYear,
  kDate,
  kMaxValue = kDate,
};

// Returns the part of the expiration date that a field of `type` holds, or
// `std::nullopt` if `type` isn't an expiration date type. Names aren't limited,
// since they can't contain digits.
std::optional<ExpirationPart> GetExpirationPart(FieldType type) {
  switch (type) {
    case CREDIT_CARD_EXP_MONTH:
      return ExpirationPart::kMonth;
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return ExpirationPart::kYear;
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return ExpirationPart::kDate;
    default:
      return std::nullopt;
  }
}

// Returns whether the <select> element `field` has an option with
// `field.value()`, and all such options have a text that is a plausible value
// of `type`. The service only sees the options' texts, so it may have picked
// the option by a text that the main frame controls.
bool HasOnlyPlausibleOptionTexts(const FormFieldData& field, FieldType type) {
  return std::ranges::any_of(field.options(),
                             [&](const SelectOption& option) {
                               return option.value == field.value();
                             }) &&
         std::ranges::all_of(field.options(), [&](const SelectOption& option) {
           return option.value != field.value() ||
                  IsPlausibleNonSensitiveCreditCardValue(
                      type, option.text, /*is_select_element=*/true);
         });
}

// Returns whether `type` is a cardholder name or expiration date type that
// `GetPlausibleNonSensitiveCreditCardTypes()` can pass to the iframe security
// policy.
bool IsSupportedNonSensitiveCreditCardType(FieldType type) {
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return true;
    default:
      return false;
  }
}

void RecordValidationResult(MainFrameCreditCardFieldValidationResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.WebView.CrossOriginMainFrameCreditCardFieldValidation", result);
}

}  // namespace

bool IsPlausibleNonSensitiveCreditCardValue(FieldType type,
                                            std::u16string_view value,
                                            bool is_select_element) {
  value = base::TrimWhitespace(value, base::TRIM_ALL);
  if (value.empty()) {
    return false;
  }
  switch (type) {
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
      return value.size() <= kMaxNameLength && IsValidNameOnCard(value);
    case CREDIT_CARD_EXP_MONTH:
      return IsPlausibleMonth(value);
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return IsPlausibleYear(value, /*allow_4_digits=*/is_select_element);
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return IsPlausibleExpirationDate(value);
    default:
      return false;
  }
}

absl::flat_hash_map<FieldGlobalId, FieldType>
GetPlausibleNonSensitiveCreditCardTypes(
    const FormStructure& form,
    base::span<const FormFieldData> filled_fields,
    const url::Origin& triggered_origin) {
  // `AutofillDriverRouter::IsSafeToFill()` only consults `field_type_map` when
  // a cross-origin frame fills a main-frame field.
  if (triggered_origin == form.main_frame_origin()) {
    return {};
  }
  absl::flat_hash_map<FieldGlobalId, FieldType> types;
  DenseSet<ExpirationPart> seen_expiration_parts;
  for (const FormFieldData& filled_field : filled_fields) {
    // `IsSafeToFill()` only consults `field_type_map` for main-frame fields,
    // and skipping subframe fields keeps them from consuming a
    // `seen_expiration_parts` slot.
    if (filled_field.origin() != form.main_frame_origin()) {
      continue;
    }
    // Skip fields that aren't in the parsed `FormStructure` (e.g. added since
    // parsing), as Autofill hasn't classified them.
    const AutofillField* field = form.GetFieldById(filled_field.global_id());
    if (!field) {
      continue;
    }
    // Only cardholder name and expiration date fields can get a type here;
    // skip other fields without recording a validation failure.
    const FieldType type = field->Type().GetCreditCardType();
    if (!IsSupportedNonSensitiveCreditCardType(type)) {
      continue;
    }
    // Reject values (or matching <select> option texts) that don't look like
    // `type`.
    if (!IsPlausibleNonSensitiveCreditCardValue(
            type, filled_field.value(), filled_field.IsSelectElement())) {
      RecordValidationResult(
          MainFrameCreditCardFieldValidationResult::kImplausibleValue);
      continue;
    }
    if (filled_field.IsSelectElement() &&
        !HasOnlyPlausibleOptionTexts(filled_field, type)) {
      RecordValidationResult(MainFrameCreditCardFieldValidationResult::
                                 kImplausibleSelectOptionText);
      continue;
    }
    // Allow at most one filled field per expiration part across the whole form
    // (including across sections, where `FormStructureRationalizer` preserves
    // multiple card sections) so a card number split across several fields by
    // the service can't pass as multiple expiration fields.
    if (std::optional<ExpirationPart> part = GetExpirationPart(type)) {
      if (!seen_expiration_parts.insert(*part).second) {
        RecordValidationResult(
            MainFrameCreditCardFieldValidationResult::kDuplicateExpirationPart);
        continue;
      }
    }
    RecordValidationResult(MainFrameCreditCardFieldValidationResult::kAllowed);
    types.emplace(filled_field.global_id(), type);
  }
  return types;
}

}  // namespace autofill
