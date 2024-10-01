// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filling_payments_util.h"

#include <optional>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_filler.h"
#include "components/autofill/core/browser/form_parsing/credit_card_field_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/select_control_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Gets the expiration month `value` inside the <select> `field`. Since `value`
// is well defined but the website's `field` option values may not be, some
// heuristics are run to cover all observed cases.
std::u16string GetExpirationMonthSelectControlValue(
    const std::u16string& value,
    const std::string& app_locale,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  // |value| is defined to be between 1 and 12, inclusively.
  int month = 0;
  if (!base::StringToInt(value, &month) || month < 1 || month > 12) {
    if (failure_to_fill) {
      *failure_to_fill += "Cannot parse month, or value is < 1 or >12. ";
    }
    return {};
  }

  // Trim the whitespace and specific prefixes used in AngularJS from the
  // select values before attempting to convert them to months.
  std::vector<std::u16string> trimmed_values(field_options.size());
  const std::u16string kNumberPrefix = u"number:";
  const std::u16string kStringPrefix = u"string:";
  for (size_t i = 0; i < field_options.size(); ++i) {
    base::TrimWhitespace(field_options[i].value, base::TRIM_ALL,
                         &trimmed_values[i]);
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kNumberPrefix,
                                           u"");
    base::ReplaceFirstSubstringAfterOffset(&trimmed_values[i], 0, kStringPrefix,
                                           u"");
  }

  if (trimmed_values.size() == 12) {
    // The select presumable only contains the year's months.
    // If the first value of the select is 0, decrement the value of `month` so
    // January is associated with 0 instead of 1.
    int first_value;
    if (base::StringToInt(trimmed_values[0], &first_value) &&
        first_value == 0) {
      --month;
    }
  } else if (trimmed_values.size() == 13) {
    // The select presumably uses the first value as a placeholder.
    int first_value;
    // If the first value is not a number or is a negative one, check the second
    // value and apply the same logic as if there was no placeholder.
    if (!base::StringToInt(trimmed_values[0], &first_value) ||
        first_value < 0) {
      int second_value;
      if (base::StringToInt(trimmed_values[1], &second_value) &&
          second_value == 0) {
        --month;
      }
    } else if (first_value == 1) {
      // If the first value of the select is 1, increment the value of |month|
      // to skip the placeholder value (January = 2).
      ++month;
    }
  }

  // Attempt to match the user's `month` with the field's value attributes.
  for (size_t i = 0; i < trimmed_values.size(); ++i) {
    int converted_value = 0;
    // We use the trimmed value to match with `month`, but the original select
    // value to fill the field (otherwise filling wouldn't work).
    if (data_util::ParseExpirationMonth(trimmed_values[i], app_locale,
                                        &converted_value) &&
        month == converted_value) {
      return field_options[i].value;
    }
  }

  // Attempt to match with each of the options' content.
  for (const SelectOption& option : field_options) {
    int converted_contents = 0;
    if (data_util::ParseExpirationMonth(option.text, app_locale,
                                        &converted_contents) &&
        month == converted_contents) {
      return option.value;
    }
  }
  if (std::optional<std::u16string> numeric_value =
          GetNumericSelectControlValue(month, field_options, failure_to_fill)) {
    return *numeric_value;
  }
  return GetSelectControlValue(value, field_options, failure_to_fill)
      .value_or(u"");
}

// Returns true if the last two digits in `year` match those in `str`.
bool LastTwoDigitsMatch(const std::u16string& year,
                        const std::u16string& option) {
  int year_int, option_int;
  return base::StringToInt(year, &year_int) &&
         base::StringToInt(option, &option_int) &&
         (year_int % 100) == (option_int % 100);
}

// Gets the year `value` in a select control to fill into the given `field` by
// comparing the last two digits of the year to the field's options.
// Returns an empty string if no value for filling was found.
std::u16string GetYearSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return *select_control_value;
  }
  if (value.size() != 2U && value.size() != 4U) {
    if (failure_to_fill) {
      *failure_to_fill += "Year to fill does not have length 2 or 4. ";
    }
    return {};
  }

  for (const SelectOption& option : field_options) {
    if (LastTwoDigitsMatch(value, option.value) ||
        LastTwoDigitsMatch(value, option.text)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Year to fill was not found in select control element. ";
  }
  return {};
}

// Gets the credit card type `value` (Visa, Mastercard, etc.) to fill into the
// given `field`. We ignore whitespace when filling credit card types to
// allow for cases such as "Master card".
// Returns an empty string if no value for filling was found.
std::u16string GetCreditCardTypeSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return *select_control_value;
  }
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValueSubstringMatch(value, /*ignore_whitespace=*/true,
                                              field_options, failure_to_fill)) {
    return *select_control_value;
  }
  if (value == l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX)) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueSubstringMatch(
                u"AmEx",
                /*ignore_whitespace=*/true, field_options, failure_to_fill)) {
      return *select_control_value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill += "Failed to fill credit card type. ";
  }
  return {};
}

std::u16string TruncateCardNumberIfNecessary(size_t card_number_offset,
                                             uint64_t field_max_length,
                                             const std::u16string& value) {
  // Take the substring of the credit card number starting from the offset
  // and ending at the field's max_length (or the entire string if
  // max_length is 0).
  // If the offset is greater than the length of the string, then the entire
  // number should be returned;
  return card_number_offset < value.length()
             ? value.substr(card_number_offset, field_max_length > 0
                                                    ? field_max_length
                                                    : std::u16string::npos)
             : value;
}

// Returns the appropriate credit card number from `credit_card`. Truncates the
// credit card number to be split across HTML form input fields depending on if
// 'field.credit_card_number_offset()' is less than the length of the credit
// card number.
std::u16string GetCreditCardNumberForInput(
    const CreditCard& credit_card,
    size_t card_number_offset,
    uint64_t field_max_length,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence) {
  std::u16string value;
  if (action_persistence == mojom::ActionPersistence::kPreview) {
    // A single field is detected when the offset begins at 0 and the field's
    // max_length can hold the entire obfuscated credit card number.
    bool is_single_field =
        (card_number_offset == 0 &&
         (field_max_length == 0 ||
          field_max_length >=
              credit_card.ObfuscatedNumberWithVisibleLastFourDigits()
                  .length()));

    // If previewing a credit card number that needs to be split, pad the number
    // to 16 digits rather than displaying a fancy string with RTL support. This
    // also returns 16 digits if there is only one field and it cannot fit the
    // longer version CC number.
    value =
        is_single_field
            ? credit_card.ObfuscatedNumberWithVisibleLastFourDigits()
            : credit_card
                  .ObfuscatedNumberWithVisibleLastFourDigitsForSplitFields();
  } else {
    value = credit_card.GetInfo(CREDIT_CARD_NUMBER, app_locale);
  }
  // Check to truncate card number based on the field's credit card number
  // offset and length of the credit card number.
  return TruncateCardNumberIfNecessary(card_number_offset, field_max_length,
                                       value);
}

// Returns the appropriate credit card number from `virtual_card`. Truncates the
// credit card number to be split across HTML form input fields depending on if
// 'field.credit_card_number_offset()' is less than the length of the credit
// card number.
std::u16string GetVirtualCardNumberForPreviewInput(
    const CreditCard& virtual_card,
    size_t card_number_offset,
    uint64_t field_max_length) {
  std::u16string value =
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE) +
      u" " + virtual_card.CardNameAndLastFourDigits();

  // |field|'s max_length truncates the credit card number to fit within.
  if (card_number_offset < value.length()) {
    // A single field is detected when the offset begins at 0 and the field's
    // max_length can hold the entire obfuscated credit card number.
    if (card_number_offset != 0 ||
        (field_max_length != 0 && field_max_length < value.length())) {
      value = virtual_card
                  .ObfuscatedNumberWithVisibleLastFourDigitsForSplitFields();
    }
    // Check to truncate card number based on the field's credit card number
    // offset and length of the credit card number.
    return TruncateCardNumberIfNecessary(card_number_offset, field_max_length,
                                         value);
  }
  return value;
}

// Returns the credit card CVC for Preview or Fill.
std::u16string GetCreditCardVerificationCodeForInput(
    const CreditCard& credit_card,
    mojom::ActionPersistence action_persistence,
    const std::u16string& cvc) {
  const std::u16string cvc_candidate =
      credit_card.cvc().empty() ? cvc : credit_card.cvc();
  if (cvc_candidate.empty()) {
    return {};
  }
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill:
      return cvc_candidate;
    // For preview, we will mask CVC with dots.
    case mojom::ActionPersistence::kPreview:
      return CreditCard::GetMidlineEllipsisPlainDots(cvc_candidate.length());
  }
}

// Gets the appropriate expiration date from the |card| for a month control
// field. (i.e. a <input type="month">)
std::u16string GetExpirationForMonthControl(const CreditCard& card) {
  return base::StrCat({card.Expiration4DigitYearAsString(), u"-",
                       card.Expiration2DigitMonthAsString()});
}

// Returns the appropriate expiration year from `credit_card` for the field.
// Uses the `field`'s type and the `field`'s max_length attribute to
// determine if the year needs to be truncated.
std::u16string GetExpirationYearForInput(const CreditCard& credit_card,
                                         FieldType field_type,
                                         uint64_t field_max_length) {
  const size_t year_length = DetermineExpirationYearLength(field_type);
  std::u16string value = year_length == 2
                             ? credit_card.Expiration2DigitYearAsString()
                             : credit_card.Expiration4DigitYearAsString();
  // In case the field's max_length is less than the length of the year, shorten
  // the year to field.max_length.
  return field_max_length != 0 && field_max_length < value.length()
             ? value.substr(value.length() - field_max_length, field_max_length)
             : value;
}

// Returns the appropriate expiration date from `credit_card` for the field
// based on the `field_type`. If the field contains a recognized date format
// string, the function follows that format. Otherwise, it uses the `field`'s
// max_length attribute to determine if the `value` needs to be truncated or
// padded. Returns an empty string in case of a failure.
std::u16string GetExpirationDateForInput(const CreditCard& credit_card,
                                         const AutofillField& field,
                                         std::string* failure_to_fill) {
  std::u16string mm = credit_card.Expiration2DigitMonthAsString();
  std::u16string yy = credit_card.Expiration2DigitYearAsString();
  std::u16string yyyy = credit_card.Expiration4DigitYearAsString();

  FieldType field_type = field.Type().GetStorableType();
  // At this point the field type is determined, so we pass it even as
  // `forced_field_type`.
  CreditCardFieldParser::ExpirationDateFormat format;
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    format = CreditCardFieldParser::DetermineExpirationDateFormat(
        field, /*fallback_type=*/field_type,
        /*server_hint=*/field_type, /*forced_field_type=*/field_type);
  } else {
    // Before the experiment, the type was not fully determined yet. That
    // happened at field filling time like in this else-branch.
    FieldType server_hint = field.server_type();
    FieldType forced_field_type = field.server_type_prediction_is_override()
                                      ? server_hint
                                      : NO_SERVER_DATA;
    FieldType fallback_type = field.Type().GetStorableType();
    format = CreditCardFieldParser::DetermineExpirationDateFormat(
        field, fallback_type, server_hint, forced_field_type);
  }

  std::u16string expiration_candidate =
      base::StrCat({mm, format.separator,
                    format.digits_in_expiration_year == 4 ? yyyy : yy});
  if (field.max_length() != 0 &&
      expiration_candidate.length() > field.max_length()) {
    if (failure_to_fill) {
      *failure_to_fill +=
          "Field to fill must have a max length of at least 4. ";
    }
    return {};
  }
  return expiration_candidate;
}

// Returns the appropriate `credit_card` value based on `field`'s type to fill
// into the input `field`.
std::u16string GetFillingValueForCreditCardForInput(
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    std::string* failure_to_fill) {
  if (field.form_control_type() == FormControlType::kInputMonth) {
    return GetExpirationForMonthControl(credit_card);
  }
  switch (FieldType storable_type = field.Type().GetStorableType()) {
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return GetCreditCardVerificationCodeForInput(credit_card,
                                                   action_persistence, cvc);
    case CREDIT_CARD_NUMBER:
      return GetCreditCardNumberForInput(
          credit_card, field.credit_card_number_offset(), field.max_length(),
          app_locale, action_persistence);
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return GetExpirationDateForInput(credit_card, field, failure_to_fill);
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return GetExpirationYearForInput(credit_card, storable_type,
                                       field.max_length());
    default:
      // All other cases handled here.
      return credit_card.GetInfo(storable_type, app_locale);
  }
}

// Replaces the digits in `value` with dots. Used for credit card preview when
// obfuscating card information to the user.
std::u16string ReplaceDigitsWithCenterDots(std::u16string value) {
  base::ranges::replace_if(
      value.begin(), value.end(),
      [](char16_t c) { return base::IsAsciiDigit(c); },
      kMidlineEllipsisPlainDot);
  return value;
}

// Returns the appropriate `virtual_card` value based on the type of `field` to
// preview into `field`.
std::u16string GetValueForVirtualCardInputPreview(
    const CreditCard& virtual_card,
    const std::string& app_locale,
    const AutofillField& field,
    std::string* failure_to_fill) {
  CHECK_EQ(virtual_card.record_type(), CreditCard::RecordType::kVirtualCard);
  switch (FieldType storable_type = field.Type().GetStorableType()) {
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      // For preview virtual card CVC, return three dots unless for American
      // Express, which uses 4-digit CVCs.
      return virtual_card.network() == kAmericanExpressCard
                 ? CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/4)
                 : CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/3);
    case CREDIT_CARD_NUMBER:
      return GetVirtualCardNumberForPreviewInput(
          virtual_card, field.credit_card_number_offset(), field.max_length());
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return ReplaceDigitsWithCenterDots(GetFillingValueForCreditCardForInput(
          virtual_card, /*cvc=*/std::u16string(), app_locale,
          /*action_persistence=*/mojom::ActionPersistence::kPreview, field,
          failure_to_fill));
    default:
      // All other cases handled here.
      return virtual_card.GetInfo(storable_type, app_locale);
  }
}

std::u16string GetFillingValueForCreditCardSelectControl(
    const std::u16string& value,
    const std::string& app_locale,
    const AutofillField& field,
    std::string* failure_to_fill) {
  switch (field.Type().GetStorableType()) {
    case CREDIT_CARD_EXP_MONTH:
      return GetExpirationMonthSelectControlValue(
          value, app_locale, field.options(), failure_to_fill);
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return GetYearSelectControlValue(value, field.options(), failure_to_fill);
    case CREDIT_CARD_TYPE:
      return GetCreditCardTypeSelectControlValue(value, field.options(),
                                                 failure_to_fill);
    default:
      return GetSelectControlValue(value, field.options(), failure_to_fill)
          .value_or(u"");
  }
}

}  // namespace

std::u16string GetFillingValueForCreditCard(
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    std::string* failure_to_fill) {
  CHECK(FieldTypeGroupSet(
            {FieldTypeGroup::kCreditCard, FieldTypeGroup::kStandaloneCvcField})
            .contains(field.Type().group()));
  std::u16string value =
      credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
              action_persistence == mojom::ActionPersistence::kPreview
          ? GetValueForVirtualCardInputPreview(credit_card, app_locale, field,
                                               failure_to_fill)
          : GetFillingValueForCreditCardForInput(credit_card, cvc, app_locale,
                                                 action_persistence, field,
                                                 failure_to_fill);

  return field.IsSelectElement() && !value.empty()
             ? GetFillingValueForCreditCardSelectControl(value, app_locale,
                                                         field, failure_to_fill)
             : value;
}

bool WillFillCreditCardNumberOrCvc(
    base::span<const FormFieldData> fields,
    base::span<const std::unique_ptr<AutofillField>> autofill_fields,
    const AutofillField& trigger_autofill_field,
    bool card_has_cvc) {
  DenseSet<FieldType> fillable_field_types({CREDIT_CARD_NUMBER});
  // Add CVC field types to `fillable_field_types` if CVC storage is enabled and
  // the card to be filled has a CVC saved.
  if (card_has_cvc && base::FeatureList::IsEnabled(
                          features::kAutofillEnableCvcStorageAndFilling)) {
    fillable_field_types.insert(CREDIT_CARD_VERIFICATION_CODE);
    fillable_field_types.insert(CREDIT_CARD_STANDALONE_VERIFICATION_CODE);
  }
  if (fillable_field_types.contains(
          trigger_autofill_field.Type().GetStorableType())) {
    return true;
  }

  // `fields` are received from the renderer and may be more up to date
  // than the `autofill_fields` stored in the cache. Therefore, we need
  // to validate for each `field` in the cache we try to fill whether it still
  // exists in the renderer and whether it is fillable.
  auto IsFillableField =
      [&fields, &trigger_autofill_field](const AutofillField& autofill_field) {
        auto field = base::ranges::find(fields, autofill_field.global_id(),
                                        &FormFieldData::global_id);
        if (field == fields.end()) {
          return false;
        }

        // Needed for FormFiller::GetFieldSkipReason() but unnecessary for this
        // case as only finding 1 fillable credit card or CVC field is needed.
        base::flat_map<FieldType, size_t> type_count;

        // TODO(crbug.com/328478565): Cover cases where filling is skipped due
        // to the iframe security policy.
        return FormFiller::GetFieldFillingSkipReason(
                   *field, autofill_field, trigger_autofill_field, type_count,
                   std::nullopt) == FieldFillingSkipReason::kNotSkipped;
      };

  auto IsFillableCreditCardNumberOrCvcField =
      [&trigger_autofill_field, &fillable_field_types,
       &IsFillableField](const std::unique_ptr<AutofillField>& autofill_field) {
        return fillable_field_types.contains(
                   autofill_field->Type().GetStorableType()) &&
               autofill_field->section() == trigger_autofill_field.section() &&
               IsFillableField(*autofill_field);
      };

  // This runs O(N^2) in the worst case, but usually there aren't too many
  // credit card number or CVC fields in a form.
  return std::ranges::any_of(autofill_fields,
                             IsFillableCreditCardNumberOrCvcField);
}

}  // namespace autofill
