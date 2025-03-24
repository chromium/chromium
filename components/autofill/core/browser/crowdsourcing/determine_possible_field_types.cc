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
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/disambiguate_possible_field_types.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

// Finds the first field in |form_structure| with |field.value|=|value|.
AutofillField* FindFirstFieldWithValue(const FormStructure& form_structure,
                                       const std::u16string& value) {
  for (const auto& field : form_structure) {
    std::u16string trimmed_value;
    base::TrimWhitespace(field->value_for_import(), base::TRIM_ALL,
                         &trimmed_value);
    if (trimmed_value == value) {
      return field.get();
    }
  }
  return nullptr;
}

// Heuristically identifies all possible credit card verification fields.
AutofillField* HeuristicallyFindCVCFieldForUpload(
    const FormStructure& form_structure) {
  // Stores a pointer to the explicitly found expiration year.
  bool found_explicit_expiration_year_field = false;

  // The first pass checks the existence of an explicitly marked field for the
  // credit card expiration year.
  for (const auto& field : form_structure) {
    const FieldTypeSet& type_set = field->possible_types();
    if (type_set.find(CREDIT_CARD_EXP_2_DIGIT_YEAR) != type_set.end() ||
        type_set.find(CREDIT_CARD_EXP_4_DIGIT_YEAR) != type_set.end()) {
      found_explicit_expiration_year_field = true;
      break;
    }
  }

  // Keeps track if a credit card number field was found.
  bool credit_card_number_found = false;

  // In the second pass, the CVC field is heuristically searched for.
  // A field is considered a CVC field, iff:
  // * it appears after the credit card number field;
  // * it has the |UNKNOWN_TYPE| prediction;
  // * it does not look like an expiration year or an expiration year was
  //   already found;
  // * it is filled with a 3-4 digit number;
  for (const auto& field : form_structure) {
    const FieldTypeSet& type_set = field->possible_types();

    // Checks if the field is of |CREDIT_CARD_NUMBER| type.
    if (type_set.find(CREDIT_CARD_NUMBER) != type_set.end()) {
      credit_card_number_found = true;
      continue;
    }
    // Skip the field if no credit card number was found yet.
    if (!credit_card_number_found) {
      continue;
    }

    // Don't consider fields that already have any prediction.
    if (type_set.find(UNKNOWN_TYPE) == type_set.end()) {
      continue;
    }
    // |UNKNOWN_TYPE| should come alone.
    DCHECK_EQ(1u, type_set.size());

    std::u16string trimmed_value;
    base::TrimWhitespace(field->value_for_import(), base::TRIM_ALL,
                         &trimmed_value);

    // Skip the field if it can be confused with a expiration year.
    if (!found_explicit_expiration_year_field &&
        IsPlausible4DigitExpirationYear(trimmed_value)) {
      continue;
    }

    // Skip the field if its value does not like a CVC value.
    if (!IsPlausibleCreditCardCVCNumber(trimmed_value)) {
      continue;
    }

    return field.get();
  }
  return nullptr;
}

// Iff the CVC of the credit card is known, find the first field with this
// value (also set |properties_mask| to |kKnownValue|). Otherwise, heuristically
// search for the CVC field if any.
AutofillField* GetBestPossibleCVCFieldForUpload(
    const FormStructure& form_structure,
    std::u16string last_unlocked_credit_card_cvc) {
  if (!last_unlocked_credit_card_cvc.empty()) {
    AutofillField* result =
        FindFirstFieldWithValue(form_structure, last_unlocked_credit_card_cvc);
    if (result) {
      result->set_properties_mask(FieldPropertiesFlags::kKnownValue);
    }
    return result;
  }

  return HeuristicallyFindCVCFieldForUpload(form_structure);
}

// Extracts the value from `field`. Then for each profile or credit card,
// identify any stored types that match the value. Runs additional heuristics
// for increased accuracy. Defaults to `{UNKNOWN_TYPE}` if no types could be
// found.
void FindAndSetPossibleFieldTypesForField(
    AutofillField& field,
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::string& app_locale) {
  std::u16string value = field.value_for_import();
  base::TrimWhitespace(value, base::TRIM_ALL, &value);

  if (!field.possible_types().empty() && value.empty()) {
    // This is a password field in a sign-in form. Skip checking its type
    // since |field->value| is not set.
    DCHECK_EQ(1u, field.possible_types().size());
    DCHECK_EQ(PASSWORD, *field.possible_types().begin());
    return;
  }
  FieldTypeSet matching_types;

  for (const AutofillProfile& profile : profiles) {
    profile.GetMatchingTypes(value, app_locale, &matching_types);
  }
  for (const CreditCard& card : credit_cards) {
    card.GetMatchingTypes(value, app_locale, &matching_types);
  }

  if (field.state_is_a_matching_type()) {
    matching_types.insert(ADDRESS_HOME_STATE);
  }
  if (matching_types.empty()) {
    matching_types.insert(UNKNOWN_TYPE);
  }
  field.set_possible_types(matching_types);
}

// For each `form` field, searches for the field value in profiles and credit
// cards and sets the field's possible types accordingly. Special heuristics are
// run for finding the CVC field.
void FindAndSetPossibleFieldTypes(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::u16string& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure& form) {
  for (size_t i = 0; i < form.field_count(); ++i) {
    FindAndSetPossibleFieldTypesForField(*form.field(i), profiles, credit_cards,
                                         app_locale);
  }

  // As CVCs are not stored, run special heuristics to detect CVC-like values.
  AutofillField* cvc_field =
      GetBestPossibleCVCFieldForUpload(form, last_unlocked_credit_card_cvc);
  if (cvc_field) {
    FieldTypeSet possible_types = cvc_field->possible_types();
    possible_types.erase(UNKNOWN_TYPE);
    possible_types.insert(CREDIT_CARD_VERIFICATION_CODE);
    cvc_field->set_possible_types(possible_types);
  }
}

// Matches a date consisting of year, month, and day in a the given string.
std::vector<std::u16string> GetMatchingCompleteDateFormats(
    std::u16string_view date) {
  std::vector<std::u16string> format_strings;
  for (std::u16string_view format :
       {// Ordering: year month day.
        u"YYYY*MM*DD", u"YY*MM*DD", u"YYYY+M+D", u"YY+M+D",
        // Ordering: month day year.
        u"MM*DD*YYYY", u"MM*DD*YY", u"M+D+YYYY", u"M+D+YY",
        // Ordering: day month year.
        u"DD*MM*YYYY", u"DD*MM*YY", u"D+M+YYYY", u"D+M+YY"}) {
    data_util::Date result;
    const char16_t* separator = nullptr;
    if (data_util::ParseDate(date, format, result, separator) &&
        data_util::IsValidDateForFormat(result, format)) {
      std::u16string instantiated_format;
      base::ReplaceChars(format, u"*+", separator, &instantiated_format);
      if (data_util::ParseDate(date, instantiated_format, result)) {
        format_strings.push_back(instantiated_format);
      }
    }
  }
  return format_strings;
}

}  // namespace

void PreProcessStateMatchingTypes(const AutofillClient& client,
                                  const std::vector<AutofillProfile>& profiles,
                                  FormStructure& form_structure) {
  for (const auto& profile : profiles) {
    std::optional<AlternativeStateNameMap::CanonicalStateName>
        canonical_state_name_from_profile =
            profile.GetAddress().GetCanonicalizedStateName();

    if (!canonical_state_name_from_profile) {
      continue;
    }

    const std::u16string& country_code = profile.GetInfo(
        AutofillType(HtmlFieldType::kCountryCode), client.GetAppLocale());

    for (auto& field : form_structure) {
      if (field->state_is_a_matching_type()) {
        continue;
      }

      std::optional<AlternativeStateNameMap::CanonicalStateName>
          canonical_state_name_from_text =
              AlternativeStateNameMap::GetCanonicalStateName(
                  base::UTF16ToUTF8(country_code), field->value_for_import());

      if (canonical_state_name_from_text &&
          canonical_state_name_from_text.value() ==
              canonical_state_name_from_profile.value()) {
        field->set_state_is_a_matching_type();
      }
    }
  }
}

void DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::u16string& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure& form) {
  for (const std::unique_ptr<AutofillField>& field : form) {
    // DeterminePossibleFieldTypesForUpload may be called multiple times. Reset
    // the values so that the first call does not affect later calls.
    field->set_possible_types({});
  }
  FindAndSetPossibleFieldTypes(profiles, credit_cards,
                               last_unlocked_credit_card_cvc, app_locale, form);
  DisambiguatePossibleFieldTypes(form);
}

std::map<FieldGlobalId, base::flat_set<std::u16string>>
DeterminePossibleFormatStringsForUpload(
    const base::span<const std::unique_ptr<AutofillField>> fields) {
  // Cheap plausibility checks if the field is relevant for date matching.
  auto may_be_interesting = [](const std::unique_ptr<AutofillField>& field) {
    return field->form_control_type() == FormControlType::kInputText &&
           (field->is_user_edited() || field->is_autofilled() ||
            field->value(ValueSemantics::kInitial) !=
                field->value(ValueSemantics::kCurrent));
  };

  // Cheap check if the field's value might contain a year, month, and day.
  auto may_be_complete_date = [&](const std::unique_ptr<AutofillField>& field) {
    static constexpr size_t kMinDateLength =
        std::u16string_view(u"1.1.25").size();
    static constexpr size_t kMaxDateLength =
        std::u16string_view(u"2025 / 12 / 31").size();
    const std::u16string& value = field->value(ValueSemantics::kCurrent);
    return kMinDateLength <= value.size() && value.size() <= kMaxDateLength &&
           std::ranges::all_of(value, [&](char16_t c) {
             return base::IsAsciiDigit(c) || data_util::IsDateSeparatorChar(c);
           });
  };

  // Cheap check if the field's value might contain a year, month, or day.
  auto may_be_part_of_date = [](const std::unique_ptr<AutofillField>& field) {
    const std::u16string& value = field->value(ValueSemantics::kCurrent);
    return 1 <= value.size() && value.size() <= 4 &&
           std::ranges::all_of(value, base::IsAsciiDigit<char16_t>);
  };

  // Cheap check if the three fields' values might together contain a year,
  // month and day.
  // TODO(crbug.com/396325496): Remove the label / separator comparisons if
  // crrev.com/c/6360977 has landed.
  auto may_be_split_date =
      [&](base::span<const std::unique_ptr<AutofillField>, 3> group) {
        return std::ranges::all_of(group, may_be_part_of_date) &&
               (group[0]->label() == group[1]->label() ||
                std::ranges::all_of(group[1]->label(),
                                    data_util::IsDateSeparatorChar)) &&
               group[1]->label() == group[2]->label();
      };

  std::map<FieldGlobalId, base::flat_set<std::u16string>> formats_by_field;

  // Match formats against individual fields.
  if (base::FeatureList::IsEnabled(
          features::kAutofillAiVoteForFormatStringsFromSingleFields)) {
    for (const std::unique_ptr<AutofillField>& field : fields) {
      if (!may_be_interesting(field) || !may_be_complete_date(field)) {
        continue;
      }
      const std::vector<std::u16string> formats =
          GetMatchingCompleteDateFormats(
              field->value(ValueSemantics::kCurrent));
      if (!formats.empty()) {
        formats_by_field.emplace(field->global_id(), std::move(formats));
      }
    }
  }

  // Match formats against groups of three consecutive fields.
  if (base::FeatureList::IsEnabled(
          features::kAutofillAiVoteForFormatStringsFromMultipleFields)) {
    for (size_t i = 0; i + 2 < fields.size(); ++i) {
      const base::span<const std::unique_ptr<AutofillField>, 3> group =
          fields.subspan(i).first<3>();
      if (!std::ranges::all_of(group, may_be_interesting) ||
          !may_be_split_date(group) ||
          !base::FeatureList::IsEnabled(
              features::kAutofillAiVoteForFormatStringsFromMultipleFields)) {
        continue;
      }
      static constexpr std::u16string_view kSeparator = u"-";
      static_assert(
          std::ranges::all_of(kSeparator, data_util::IsDateSeparatorChar));
      const std::u16string date =
          base::JoinString({group[0]->value(ValueSemantics::kCurrent),
                            group[1]->value(ValueSemantics::kCurrent),
                            group[2]->value(ValueSemantics::kCurrent)},
                           kSeparator);
      for (const std::u16string& format :
           GetMatchingCompleteDateFormats(date)) {
        const std::vector<std::u16string> partial_formats = base::SplitString(
            format, kSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        if (partial_formats.size() == 3) {
          for (size_t j = 0; j < 3; ++j) {
            formats_by_field[group[j]->global_id()].insert(
                std::move(partial_formats[j]));
          }
        }
      }
    }
  }
  return formats_by_field;
}

}  // namespace autofill
