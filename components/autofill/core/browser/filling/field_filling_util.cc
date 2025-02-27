// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/field_filling_util.h"

#include <string>

#include "base/i18n/string_search.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

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

}  // namespace autofill
