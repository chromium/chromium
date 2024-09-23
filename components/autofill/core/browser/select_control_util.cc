// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/select_control_util.h"

#include "base/i18n/string_search.h"
#include "base/strings/string_split.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

int FindShortestSubstringMatchInSelect(
    const std::u16string& value,
    bool ignore_whitespace,
    base::span<const SelectOption> field_options) {
  int best_match = -1;

  std::u16string value_stripped =
      ignore_whitespace ? RemoveWhitespace(value) : value;
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents searcher(
      value_stripped);
  for (size_t i = 0; i < field_options.size(); ++i) {
    const SelectOption& option = field_options[i];
    std::u16string option_value =
        ignore_whitespace ? RemoveWhitespace(option.value) : option.value;
    std::u16string option_text =
        ignore_whitespace ? RemoveWhitespace(option.text) : option.text;
    if (searcher.Search(option_value, nullptr, nullptr) ||
        searcher.Search(option_text, nullptr, nullptr)) {
      if (best_match == -1 ||
          field_options[best_match].value.size() > option.value.size()) {
        best_match = i;
      }
    }
  }
  return best_match;
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
  if (int best_match = FindShortestSubstringMatchInSelect(
          value, ignore_whitespace, field_options);
      best_match >= 0) {
    return field_options[best_match].value;
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

}  // namespace autofill
