// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/field_filling_util.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/proto/states.pb.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

// Helper method to normalize the `admin_area` for the given `country_code`.
// The value in `admin_area` will be overwritten.
bool NormalizeAdminAreaForCountryCode(std::u16string& admin_area,
                                      const std::string& country_code,
                                      AddressNormalizer& address_normalizer) {
  if (admin_area.empty() || country_code.empty()) {
    return false;
  }

  AutofillProfile tmp_profile =
      AutofillProfile(AddressCountryCode(country_code));
  tmp_profile.SetRawInfo(ADDRESS_HOME_STATE, admin_area);
  if (!address_normalizer.NormalizeAddressSync(&tmp_profile)) {
    return false;
  }

  admin_area = tmp_profile.GetRawInfo(ADDRESS_HOME_STATE);
  return true;
}

// Returns the SelectOption::value of `field_options` that best matches the
// normalized `value`. Returns an empty string if no match is found.
// Normalization is relative to the `country_code` and to `address_normalizer`.
std::u16string GetNormalizedStateSelectControlValue(
    std::u16string value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer& address_normalizer,
    std::string* failure_to_fill) {
  // We attempt to normalize `value`. If normalization was not successful, it
  // means the rules were probably not loaded. Give up. Note that the normalizer
  // will fetch the rule next time it's called.
  // TODO(crbug.com/40551524): We should probably sanitize |value| before
  // normalizing.
  if (!NormalizeAdminAreaForCountryCode(value, country_code,
                                        address_normalizer)) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not normalize admin area for country code. ";
    }
    return {};
  }

  // If successful, try filling the normalized value with the existing field
  // |options|.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return *select_control_value;
  }

  // Normalize `field_options` using a copy.
  // TODO(crbug.com/40551524): We should probably sanitize the values in
  // `field_options_copy` before normalizing.
  bool normalized = false;
  std::vector<SelectOption> field_options_copy(field_options.begin(),
                                               field_options.end());
  for (SelectOption& option : field_options_copy) {
    normalized |= NormalizeAdminAreaForCountryCode(option.value, country_code,
                                                   address_normalizer);
    normalized |= NormalizeAdminAreaForCountryCode(option.text, country_code,
                                                   address_normalizer);
  }

  // Try filling the normalized value with the existing `field_options_copy`.
  size_t best_match_index = 0;
  if (normalized && GetSelectControlValue(value, field_options_copy,
                                          failure_to_fill, &best_match_index)) {
    // `best_match_index` now points to the option in `field->options`
    // that corresponds to our best match.
    return field_options[best_match_index].value;
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not set normalized state in control element. ";
  }
  return {};
}

std::optional<std::u16string> GetSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill,
    size_t* best_match_index) {
  l10n::CaseInsensitiveCompare compare;

  std::u16string best_match;
  for (size_t i = 0; i < field_options.size(); ++i) {
    const SelectOption& option = field_options[i];
    if (value == option.value || value == option.text) {
      // An exact match, use it.
      best_match = option.value;
      if (best_match_index) {
        *best_match_index = i;
      }
      break;
    }

    if (compare.StringsEqual(value, option.value) ||
        compare.StringsEqual(value, option.text)) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      best_match = option.value;
      if (best_match_index) {
        *best_match_index = i;
      }
    }
  }

  if (best_match.empty()) {
    if (failure_to_fill) {
      *failure_to_fill +=
          "Did not find value to fill in select control element. ";
    }
    return std::nullopt;
  }

  return best_match;
}

std::optional<std::u16string> GetSelectControlValueSubstringMatch(
    const std::u16string& value,
    bool ignore_whitespace,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  if (auto best_match = FindShortestSubstringMatchInSelect(
          value, ignore_whitespace, field_options)) {
    return field_options[best_match.value()].value;
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find substring match for filling select control element. ";
  }

  return std::nullopt;
}

std::optional<std::u16string> GetSelectControlValueTokenMatch(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  const auto tokenize = [](const std::u16string& str) {
    return base::SplitString(str, base::kWhitespaceASCIIAs16,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  };
  l10n::CaseInsensitiveCompare compare;
  const auto equals_value = [&](const std::u16string& rhs) {
    return compare.StringsEqual(value, rhs);
  };
  for (const SelectOption& option : field_options) {
    if (std::ranges::any_of(tokenize(option.value), equals_value) ||
        std::ranges::any_of(tokenize(option.text), equals_value)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find token match for filling select control element. ";
  }

  return std::nullopt;
}

std::optional<std::u16string> GetNumericSelectControlValue(
    int value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  for (const SelectOption& option : field_options) {
    int num;
    if ((base::StringToInt(option.value, &num) && num == value) ||
        (base::StringToInt(option.text, &num) && num == value)) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find numeric value to fill in select control element. ";
  }
  return std::nullopt;
}

std::u16string GetObfuscatedValue(const std::u16string& value) {
  // Same obfuscation symbol as used for credit cards - see also credit_card.h.
  //  - \u2022 - Bullet.
  //  - \u2006 - SIX-PER-EM SPACE (small space between bullets).
  //  - \u2060 - WORD-JOINER (makes obfuscated string indivisible).
  static constexpr char16_t kDot[] = u"\u2022\u2060\u2006\u2060";
  // This is only an approximation of the number of the actual unicode
  // characters - if we want to match the length exactly, we would need to use
  // `base::CountUnicodeCharacters`.
  const size_t obfuscation_length = value.size();
  std::u16string result;
  result.reserve(sizeof(kDot) * obfuscation_length);
  for (size_t i = 0; i < obfuscation_length; ++i) {
    result.append(kDot);
  }
  return result;
}

// Gets the country value to fill in a select control.
// Returns an empty string if no value for filling was found.
std::u16string GetCountrySelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    std::string* failure_to_fill) {
  // Search for exact matches.
  if (std::optional<std::u16string> select_control_value =
          GetSelectControlValue(value, field_options, failure_to_fill)) {
    return *select_control_value;
  }
  std::string country_code = CountryNames::GetInstance()->GetCountryCode(value);
  if (country_code.empty()) {
    if (failure_to_fill) {
      *failure_to_fill += "Cannot fill empty country code. ";
    }
    return {};
  }

  // Sometimes options contain a country name and phone country code (e.g.
  // "Germany (+49)"). This can happen if such a <select> is annotated as
  // autocomplete="tel-country-code". The following lambda strips the phone
  // country code so that the remainder ideally matches a country name.
  auto strip_phone_country_code =
      [](const std::u16string& value) -> std::u16string {
    static base::NoDestructor<std::unique_ptr<const RE2>> regex_pattern(
        std::make_unique<const RE2>("[(]?(?:00|\\+)\\s*[1-9]\\d{0,3}[)]?"));
    std::string u8string = base::UTF16ToUTF8(value);
    if (RE2::Replace(&u8string, **regex_pattern, "")) {
      return base::UTF8ToUTF16(
          base::TrimWhitespaceASCII(u8string, base::TRIM_ALL));
    }
    return value;
  };

  for (const SelectOption& option : field_options) {
    // Canonicalize each <option> value to a country code, and compare to the
    // target country code.
    if (country_code == CountryNames::GetInstance()->GetCountryCode(
                            strip_phone_country_code(option.value)) ||
        country_code == CountryNames::GetInstance()->GetCountryCode(
                            strip_phone_country_code(option.text))) {
      return option.value;
    }
  }

  if (failure_to_fill) {
    *failure_to_fill +=
        "Did not find country to fill in select control element. ";
  }
  return {};
}

std::u16string GetStateTextForInput(const std::u16string& state_value,
                                    const std::string& country_code,
                                    uint64_t field_max_length,
                                    std::string* failure_to_fill) {
  if (field_max_length == 0 || field_max_length >= state_value.size()) {
    // Return the state value directly.
    return state_value;
  }
  std::optional<StateEntry> state =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode(country_code),
          AlternativeStateNameMap::StateName(state_value));
  if (state) {
    // Return the canonical state name if possible.
    if (state->has_canonical_name() && !state->canonical_name().empty() &&
        field_max_length >= state->canonical_name().size()) {
      return base::UTF8ToUTF16(state->canonical_name());
    }
    // Return the abbreviation if possible.
    for (const auto& abbreviation : state->abbreviations()) {
      if (!abbreviation.empty() && field_max_length >= abbreviation.size()) {
        return base::i18n::ToUpper(base::UTF8ToUTF16(abbreviation));
      }
    }
  }
  // Return with the state abbreviation.
  std::u16string abbreviation;
  state_names::GetNameAndAbbreviation(state_value, nullptr, &abbreviation);
  if (!abbreviation.empty() && field_max_length >= abbreviation.size()) {
    return base::i18n::ToUpper(abbreviation);
  }
  if (failure_to_fill) {
    *failure_to_fill += "Could not fit raw state nor abbreviation. ";
  }
  return {};
}

// Gets the state value to fill in a select control.
// Returns an empty string if no value for filling was found.
std::u16string GetStateSelectControlValue(
    const std::u16string& value,
    base::span<const SelectOption> field_options,
    const std::string& country_code,
    AddressNormalizer* address_normalizer,
    std::string* failure_to_fill) {
  std::vector<std::u16string> abbreviations;
  std::vector<std::u16string> full_names;

  // Fetch the corresponding entry from AlternativeStateNameMap.
  std::optional<StateEntry> state_entry =
      AlternativeStateNameMap::GetInstance()->GetEntry(
          AlternativeStateNameMap::CountryCode(country_code),
          AlternativeStateNameMap::StateName(value));

  // State abbreviations will be empty for non-US countries.
  if (state_entry) {
    for (const std::string& abbreviation : state_entry->abbreviations()) {
      if (!abbreviation.empty()) {
        abbreviations.push_back(base::UTF8ToUTF16(abbreviation));
      }
    }
    if (state_entry->has_canonical_name()) {
      full_names.push_back(base::UTF8ToUTF16(state_entry->canonical_name()));
    }
    for (const std::string& alternative_name :
         state_entry->alternative_names()) {
      full_names.push_back(base::UTF8ToUTF16(alternative_name));
    }
  } else {
    if (value.size() > 2) {
      full_names.push_back(value);
    } else if (!value.empty()) {
      abbreviations.push_back(value);
    }
  }

  std::u16string state_name;
  std::u16string state_abbreviation;
  state_names::GetNameAndAbbreviation(value, &state_name, &state_abbreviation);

  full_names.push_back(std::move(state_name));
  if (!state_abbreviation.empty()) {
    abbreviations.push_back(std::move(state_abbreviation));
  }

  // Remove `abbreviations` from the `full_names` as a precautionary measure in
  // case the `AlternativeStateNameMap` contains bad data.
  std::ranges::sort(abbreviations);
  std::erase_if(full_names, [&](const std::u16string& full_name) {
    return full_name.empty() ||
           std::ranges::binary_search(abbreviations, full_name);
  });

  // Try an exact match of the abbreviation first.
  for (const std::u16string& abbreviation : abbreviations) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValue(abbreviation, field_options,
                                  failure_to_fill)) {
      return *select_control_value;
    }
  }

  // Try an exact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValue(full, field_options, failure_to_fill)) {
      return *select_control_value;
    }
  }

  // Try an inexact match of the full name.
  for (const std::u16string& full : full_names) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueSubstringMatch(
                full, /*ignore_whitespace=*/false, field_options,
                failure_to_fill)) {
      return *select_control_value;
    }
  }

  // Try an inexact match of the abbreviation name.
  for (const std::u16string& abbreviation : abbreviations) {
    if (std::optional<std::u16string> select_control_value =
            GetSelectControlValueTokenMatch(abbreviation, field_options,
                                            failure_to_fill)) {
      return *select_control_value;
    }
  }

  if (!address_normalizer) {
    if (failure_to_fill) {
      *failure_to_fill += "Could not fill state in select control element. ";
    }
    return {};
  }

  // Try to match a normalized `value` of the state and the `field_options`.
  return GetNormalizedStateSelectControlValue(
      value, field_options, country_code, *address_normalizer, failure_to_fill);
}

}  // namespace autofill
