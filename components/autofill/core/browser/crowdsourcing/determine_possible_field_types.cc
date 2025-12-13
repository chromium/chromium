// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/disambiguate_possible_field_types.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"

namespace autofill {

using one_time_tokens::OneTimeToken;

namespace {

// Returns a vector that contains all `{date, format}` for which `str` contains
// `date` in `format`.
std::vector<std::pair<data_util::Date, std::u16string>>
GetMatchingCompleteDateAndFormats(std::u16string_view str) {
  std::vector<std::pair<data_util::Date, std::u16string>> dates_and_formats;
  for (std::u16string_view format :
       {// Ordering: year month day.
        u"YYYY*MM*DD", u"YY*MM*DD", u"YYYY+M+D", u"YY+M+D",
        // Ordering: month day year.
        u"MM*DD*YYYY", u"MM*DD*YY", u"M+D+YYYY", u"M+D+YY",
        // Ordering: day month year.
        u"DD*MM*YYYY", u"DD*MM*YY", u"D+M+YYYY", u"D+M+YY"}) {
    data_util::Date date;
    const char16_t* separator = nullptr;
    if (data_util::ParseDate(str, format, date, separator) &&
        data_util::IsValidDateForFormat(date, format)) {
      std::u16string instantiated_format;
      base::ReplaceChars(format, u"*+", separator, &instantiated_format);
      if (data_util::ParseDate(str, instantiated_format, date)) {
        dates_and_formats.emplace_back(date, std::move(instantiated_format));
      }
    }
  }
  return dates_and_formats;
}

// Extracts the dates and their format strings in `fields`:
// - It adds the format strings to `PossibleTypes::formats`.
// - It returns the dates, together with a pointer to the `PossibleTypes` of
//   each field that contributes a part of the date.
//
// For example:
//
// For a field #0 with value "09/03/2025", it sets
//   pt[0].format_strings = {u"DD/MM/YYYY", u"MM/DD/YYYY"}
// and returns
//   {{{2025,03,09}, &pt[0]}}
//   {{{2025,09,03}, &pt[0]}}
//
// For a field #0 with value "01/01/01", it sets
//   pt[0].format_strings = {u"DD/MM/YY", u"MM/DD/YY", u"YY/MM/DD"}
// and returns
//   {{{2001,01,01}, &pt[0]}}
//
// For three consecutive fields with values "09", "03", "2025", it sets
//   pt[0].format_strings = {u"DD", u"MM"}
//   pt[1].format_strings = {u"DD", u"MM"}
//   pt[2].format_strings = {u"YYYY"}
// and returns
//   {{{2025,03,09}, pt[0]}, {2025,09,03, &pt[0]}}}
//   {{{2025,03,09}, pt[1]}, {2025,09,03, &pt[1]}}}
//   {{{2025,03,09}, pt[2]}, {2025,09,03, &pt[2]}}}
base::flat_set<std::pair<data_util::Date, PossibleTypes*>>
FindDatesAndSetFormatStrings(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<PossibleTypes> possible_types) {
  // Cheap plausibility checks if the field is relevant for date matching.
  auto may_be_interesting = [](const std::unique_ptr<AutofillField>& field) {
    return field->form_control_type() == FormControlType::kInputText &&
           (field->is_user_edited() || field->is_autofilled() ||
            field->initial_value() != field->value());
  };

  // Cheap check if the field's value might contain a year, month, and day.
  auto may_be_complete_date = [&](const std::unique_ptr<AutofillField>& field) {
    static constexpr size_t kMinDateLength =
        std::u16string_view(u"1.1.25").size();
    static constexpr size_t kMaxDateLength =
        std::u16string_view(u"2025 / 12 / 31").size();
    const std::u16string& value = field->value();
    return kMinDateLength <= value.size() && value.size() <= kMaxDateLength &&
           std::ranges::all_of(value, [&](char16_t c) {
             return base::IsAsciiDigit(c) || data_util::IsDateSeparatorChar(c);
           });
  };

  // Cheap check if the field's value might contain a year, month, or day.
  auto may_be_part_of_date = [](const std::unique_ptr<AutofillField>& field) {
    const std::u16string& value = field->value();
    return 1 <= value.size() && value.size() <= 4 &&
           std::ranges::all_of(value, base::IsAsciiDigit<char16_t>);
  };

  // Cheap check if the three fields' values might together contain a year,
  // month and day.
  auto may_be_split_date =
      [&](base::span<const std::unique_ptr<AutofillField>, 3> group) {
        return std::ranges::all_of(group, may_be_part_of_date) &&
               (group[0]->label() == group[1]->label() ||
                group[1]->label().empty()) &&
               group[1]->label() == group[2]->label();
      };

  std::vector<std::pair<data_util::Date, PossibleTypes*>> dates;

  // Match formats against individual fields.
  for (auto [field, pt] : base::zip(fields, possible_types)) {
    if (!may_be_interesting(field) || !may_be_complete_date(field)) {
      continue;
    }
    for (auto& [date, format] :
         GetMatchingCompleteDateAndFormats(field->value())) {
      pt.formats.emplace(FormatString_Type_DATE, std::move(format));
      dates.emplace_back(date, &pt);
    }
  }

  // Match formats against groups of three consecutive fields.
  for (size_t i = 0; i + 2 < fields.size(); ++i) {
    const base::span<const std::unique_ptr<AutofillField>, 3> group =
        fields.subspan(i).first<3>();
    if (!std::ranges::all_of(group, may_be_interesting) ||
        !may_be_split_date(group)) {
      continue;
    }
    static constexpr std::u16string_view kSeparator = u"-";
    static_assert(
        std::ranges::all_of(kSeparator, data_util::IsDateSeparatorChar));
    const std::u16string maybe_full_date = base::JoinString(
        {group[0]->value(), group[1]->value(), group[2]->value()}, kSeparator);
    for (auto& [full_date, full_format] :
         GetMatchingCompleteDateAndFormats(maybe_full_date)) {
      std::vector<std::u16string> partial_formats = base::SplitString(
          full_format, kSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      if (partial_formats.size() == 3) {
        for (size_t j = 0; j < 3; ++j) {
          possible_types[i + j].formats.emplace(FormatString_Type_DATE,
                                                std::move(partial_formats[j]));
          dates.emplace_back(full_date, &possible_types[i + j]);
        }
      }
    }
  }
  return dates;
}

// Adds `CREDIT_CARD_VERIFICATION_CODE` to the possible types of fields whose
// value is `last_unlocked_credit_card_cvc` or looks like a CVC.
void FindAndSetPossibleCvcFieldTypes(
    std::u16string_view last_unlocked_credit_card_cvc,
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<PossibleTypes> possible_types) {
  if (!last_unlocked_credit_card_cvc.empty()) {
    for (auto [field, pt] : base::zip(fields, possible_types)) {
      if (last_unlocked_credit_card_cvc ==
          base::TrimWhitespace(field->value_for_import(), base::TRIM_ALL)) {
        pt.types.insert(CREDIT_CARD_VERIFICATION_CODE);
        return;
      }
    }
  }

  // The first pass checks the existence of an explicitly marked field for the
  // credit card expiration year.
  const bool found_explicit_expiration_year_field =
      std::ranges::any_of(possible_types, [](const PossibleTypes& pt) {
        return pt.types.contains_any(
            {CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_EXP_4_DIGIT_YEAR});
      });

  // Keeps track if a credit card number field was found.
  bool credit_card_number_found = false;

  // In the second pass, the CVC field is heuristically searched for.
  // A field is considered a CVC field, iff:
  // * it appears after the credit card number field;
  // * it has no prediction yet;
  // * it does not look like an expiration year or an expiration year was
  //   already found;
  // * it is filled with a 3-4 digit number;
  for (auto [field, pt] : base::zip(fields, possible_types)) {
    if (pt.types.contains(CREDIT_CARD_NUMBER)) {
      credit_card_number_found = true;
      continue;
    }
    if (!credit_card_number_found) {
      continue;
    }
    if (!pt.types.empty()) {
      continue;
    }

    const std::u16string& value = field->value_for_import();
    const std::u16string_view trimmed_value =
        base::TrimWhitespace(value, base::TRIM_ALL);

    // Skip the field if it can be confused with a expiration year.
    if (!found_explicit_expiration_year_field &&
        IsPlausible4DigitExpirationYear(trimmed_value)) {
      continue;
    }
    if (!IsPlausibleCreditCardCVCNumber(trimmed_value)) {
      continue;
    }

    pt.types.insert(CREDIT_CARD_VERIFICATION_CODE);
  }
}

// Returns the FieldTypes for which the given EntityInstance defines a non-empty
// value.
FieldTypeSet GetAvailableAutofillAiFieldTypes(
    base::span<const EntityInstance> entities,
    const std::string& app_locale) {
  CHECK(base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema));
  FieldTypeSet types;
  for (const EntityInstance& entity : entities) {
    for (const AttributeInstance& attribute : entity.attributes()) {
      for (FieldType field_type : attribute.type().field_subtypes()) {
        bool is_empty = normalization::HasOnlySkippableCharacters(
            attribute.GetInfo(field_type, app_locale, std::nullopt));
        if (!is_empty) {
          types.insert(field_type);
        }
      }
    }
  }
  return types;
}

// Scans the given `entities` for values that match `value_u16`. It adds the
// matching `FieldType` to `PossibleTypes::types` and, if applicable, a format
// string to `PossibleTypes::format`.
void AddPossibleAutofillAiTypes(base::span<const EntityInstance> entities,
                                std::u16string_view value_u16,
                                const std::string& app_locale,
                                PossibleTypes& pt) {
  CHECK(base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema));

  if (normalization::HasOnlySkippableCharacters(value_u16)) {
    return;
  }

  const std::u16string& value_in_field =
      normalization::NormalizeForComparison(value_u16);
  for (const EntityInstance& entity : entities) {
    for (const AttributeInstance& attribute : entity.attributes()) {
      for (const FieldType field_type : attribute.type().field_subtypes()) {
        const std::u16string& value_on_file =
            normalization::NormalizeForComparison(
                attribute.GetInfo(field_type, app_locale, std::nullopt));

        // Test if `value_in_field` and `value_on_file` match.
        bool full_match =
            AutofillProfileComparator::Compare(value_in_field, value_on_file);
        if (full_match) {
          pt.types.insert(field_type);
          if (IsAffixFormatStringEnabledForType(field_type) &&
              base::FeatureList::IsEnabled(
                  features::kAutofillAiVoteForFormatStringsForAffixes)) {
            pt.formats.emplace(FormatString_Type_AFFIX, u"0");
          }
          if (field_type == FLIGHT_RESERVATION_FLIGHT_NUMBER &&
              base::FeatureList::IsEnabled(
                  features::kAutofillAiVoteForFormatStringsForFlightNumbers)) {
            pt.formats.emplace(FormatString_Type_FLIGHT_NUMBER, u"F");
          }
        }

        // Test if `value_in_field` is an affix of `value_on_file`.
        if (IsAffixFormatStringEnabledForType(field_type) &&
            value_in_field.size() < value_on_file.size() &&
            value_in_field.size() >=
                data_util::kMinAffixLengthForFormatString &&
            value_in_field.size() <=
                data_util::kMaxAffixLengthForFormatString &&
            base::FeatureList::IsEnabled(
                features::kAutofillAiVoteForFormatStringsForAffixes)) {
          if (value_on_file.starts_with(value_in_field)) {
            pt.types.insert(field_type);
            pt.formats.emplace(FormatString_Type_AFFIX,
                               base::NumberToString16(value_in_field.size()));
          }
          if (value_on_file.ends_with(value_in_field)) {
            pt.types.insert(field_type);
            pt.formats.emplace(
                FormatString_Type_AFFIX,
                base::NumberToString16(
                    -1 * static_cast<int>(value_in_field.size())));
          }
        }

        if (field_type == FLIGHT_RESERVATION_FLIGHT_NUMBER &&
            base::FeatureList::IsEnabled(
                features::kAutofillAiVoteForFormatStringsForFlightNumbers)) {
          if (value_in_field.size() == 2 &&
              value_on_file.starts_with(value_in_field)) {
            pt.types.insert(field_type);
            pt.formats.emplace(FormatString_Type_FLIGHT_NUMBER, u"A");
          } else if (value_on_file.size() > 3 &&
                     value_on_file.substr(2) == value_in_field) {
            pt.types.insert(field_type);
            pt.formats.emplace(FormatString_Type_FLIGHT_NUMBER, u"N");
          }
        }
      }
    }
  }
}

void FindAndSetPossibleDateFieldTypesAndFormatStrings(
    base::span<const EntityInstance> entities,
    const std::string& app_locale,
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<PossibleTypes> possible_types) {
  base::flat_set<std::pair<data_util::Date, PossibleTypes*>> dates =
      FindDatesAndSetFormatStrings(fields, possible_types);

  for (const EntityInstance& entity : entities) {
    for (const AttributeInstance& attribute : entity.attributes()) {
      for (const FieldType field_type : attribute.type().field_subtypes()) {
        if (!IsDateFieldType(field_type)) {
          continue;
        }
        data_util::Date date;
        if (data_util::ParseDate(attribute.GetCompleteInfo(app_locale),
                                 u"YYYY-MM-DD", date)) {
          auto get_date = [](const auto& p) { return p.first; };
          for (auto& [same_date, pt] :
               std::ranges::equal_range(dates, date, {}, get_date)) {
            pt->types.insert(field_type);
          }
        }
      }
    }
  }
}

// Matches the value from `field` against the values stored in the given
// profiles etc.
PossibleTypes GetPossibleTypes(
    const AutofillField& field,
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    const std::set<FieldGlobalId> fields_that_match_state,
    const std::string& app_locale) {
  std::u16string value_u16 = field.value_for_import();
  base::TrimWhitespace(value_u16, base::TRIM_ALL, &value_u16);

  PossibleTypes pt;

  for (const AutofillProfile& profile : profiles) {
    profile.GetMatchingTypes(value_u16, app_locale, &pt.types);
  }
  if (fields_that_match_state.contains(field.global_id())) {
    pt.types.insert(ADDRESS_HOME_STATE);
  }

  for (const CreditCard& card : credit_cards) {
    card.GetMatchingTypes(value_u16, app_locale, &pt.types);
  }

  // Do not issue loyalty card votes on values matching the email format.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableLoyaltyCardsFilling) &&
      !IsValidEmailAddress(value_u16)) {
    const std::string value_u8 = base::UTF16ToUTF8(value_u16);
    for (const LoyaltyCard& card : loyalty_cards) {
      if (value_u8 == card.loyalty_card_number()) {
        pt.types.insert(LOYALTY_MEMBERSHIP_ID);
      }
    }
  }

  if (base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema)) {
    AddPossibleAutofillAiTypes(entities, value_u16, app_locale, pt);
  }

  return pt;
}

void FindAndSetPossibleOtpFieldTypes(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<const OneTimeToken> recent_otps,
    base::span<PossibleTypes> possible_types) {
  if (recent_otps.empty()) {
    return;
  }

  for (auto [field, pt] : base::zip(fields, possible_types)) {
    std::u16string field_value = field->value();
    base::TrimWhitespace(field_value, base::TRIM_ALL, &field_value);
    const std::string field_value_u8 = base::UTF16ToUTF8(field_value);

    // Check if the field value matches any of the recent OTPs.
    for (const OneTimeToken& otp : recent_otps) {
      if (field_value_u8 == otp.value()) {
        pt.types.insert(ONE_TIME_CODE);
        return;
      }
    }
  }
}

}  // namespace

PossibleTypes::PossibleTypes() = default;
PossibleTypes::PossibleTypes(PossibleTypes&&) = default;
PossibleTypes& PossibleTypes::operator=(PossibleTypes&&) = default;
PossibleTypes::~PossibleTypes() = default;

std::set<FieldGlobalId> PreProcessStateMatchingTypes(
    base::span<const AutofillProfile*> profiles,
    base::span<const std::unique_ptr<AutofillField>> fields,
    const std::string& app_locale) {
  std::set<FieldGlobalId> fields_that_match_state;
  for (const auto* profile : profiles) {
    std::optional<AlternativeStateNameMap::CanonicalStateName>
        canonical_state_name_from_profile =
            profile->GetAddress().GetCanonicalizedStateName();

    if (!canonical_state_name_from_profile) {
      continue;
    }

    const std::u16string& country_code = profile->GetInfo(
        AutofillType(ADDRESS_HOME_COUNTRY, /*is_country_code=*/true),
        app_locale);

    for (auto& field : fields) {
      if (fields_that_match_state.contains(field->global_id())) {
        continue;
      }

      std::optional<AlternativeStateNameMap::CanonicalStateName>
          canonical_state_name_from_text =
              AlternativeStateNameMap::GetCanonicalStateName(
                  base::UTF16ToUTF8(country_code), field->value_for_import());

      if (canonical_state_name_from_text &&
          canonical_state_name_from_text.value() ==
              canonical_state_name_from_profile.value()) {
        fields_that_match_state.insert(field->global_id());
      }
    }
  }
  return fields_that_match_state;
}

std::vector<PossibleTypes> DeterminePossibleFieldTypesForUpload(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    const std::set<FieldGlobalId>& fields_that_match_state,
    std::u16string_view last_unlocked_credit_card_cvc,
    base::span<const OneTimeToken> recent_otps,
    const std::string& app_locale,
    base::span<const std::unique_ptr<AutofillField>> fields) {
  std::vector<PossibleTypes> possible_types;
  possible_types.resize(fields.size());

  // Most type detection happens in this loop.
  for (auto [field, pt] : base::zip(fields, possible_types)) {
    pt = GetPossibleTypes(*field, profiles, credit_cards, entities,
                          loyalty_cards, fields_that_match_state, app_locale);
  }

  // Date detection is not part of the above loop because dates can span
  // multiple fields.
  FindAndSetPossibleDateFieldTypesAndFormatStrings(entities, app_locale, fields,
                                                   possible_types);

  // As CVCs are not stored, run special heuristics to detect CVC-like values.
  FindAndSetPossibleCvcFieldTypes(last_unlocked_credit_card_cvc, fields,
                                  possible_types);

  if (!recent_otps.empty() &&
      base::FeatureList::IsEnabled(features::kAutofillSmsOtpCrowdsourcing)) {
    // OTPs are not stored, run special logic to detect OTP values.
    FindAndSetPossibleOtpFieldTypes(fields, recent_otps, possible_types);
  }

  for (auto [field, pt] : base::zip(fields, possible_types)) {
    if (pt.types.empty()) {
      pt.types = {UNKNOWN_TYPE};
    }
  }

  return DisambiguatePossibleFieldTypes(fields, std::move(possible_types));
}

FieldTypeSet DetermineAvailableFieldTypes(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    std::u16string_view last_unlocked_credit_card_cvc,
    base::span<const OneTimeToken> recent_otps,
    const std::string& app_locale) {
  FieldTypeSet types;
  for (const AutofillProfile& profile : profiles) {
    profile.GetNonEmptyTypes(app_locale, &types);
  }

  for (const CreditCard& card : credit_cards) {
    card.GetNonEmptyTypes(app_locale, &types);
  }
  // As CVC is not stored, treat it separately.
  if (!last_unlocked_credit_card_cvc.empty() ||
      types.contains(CREDIT_CARD_NUMBER)) {
    types.insert(CREDIT_CARD_VERIFICATION_CODE);
  }

  if (base::FeatureList::IsEnabled(features::kAutofillAiWithDataSchema)) {
    types.insert_all(GetAvailableAutofillAiFieldTypes(entities, app_locale));
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableLoyaltyCardsFilling) &&
      !loyalty_cards.empty()) {
    types.insert(LOYALTY_MEMBERSHIP_ID);
  }

  if (!recent_otps.empty() &&
      base::FeatureList::IsEnabled(features::kAutofillSmsOtpCrowdsourcing)) {
    types.insert(ONE_TIME_CODE);
  }
  return types;
}

base::flat_set<std::pair<data_util::Date, PossibleTypes*>>
FindDatesAndSetFormatStringsForTesting(  // IN-TEST
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<PossibleTypes> possible_types) {
  return FindDatesAndSetFormatStrings(fields, possible_types);
}

}  // namespace autofill
