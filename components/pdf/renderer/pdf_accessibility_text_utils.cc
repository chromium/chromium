// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_text_utils.h"

#include <algorithm>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "pdf/accessibility_structs.h"

namespace pdf {

namespace {

// Roman numerals for the ones place, from 0 ("") to 9.
constexpr auto kRomanNumeralOnesPlace =
    base::MakeFixedFlatMap<std::string_view, int>({
        {"", 0},
        {"i", 1},
        {"ii", 2},
        {"iii", 3},
        {"iv", 4},
        {"v", 5},
        {"vi", 6},
        {"vii", 7},
        {"viii", 8},
        {"ix", 9},
    });

// `ParseRomanNumeral()` only reads "X" in the tens place. 40 needs "L".
static_assert(kMaxRomanNumeralValue < 40);

}  // namespace

bool IsWhitespace(const chrome_pdf::AccessibilityCharInfo& char_info) {
  return base::IsUnicodeWhitespace(char_info.unicode_character);
}

bool IsLeftToRightRun(const chrome_pdf::AccessibilityTextRunInfo& run) {
  // `kNone` is assigned to single-character runs, whose direction cannot be
  // inferred. Treat them as LTR, the default, like `PDFiumPage` does.
  return run.direction ==
             chrome_pdf::AccessibilityTextDirection::kLeftToRight ||
         run.direction == chrome_pdf::AccessibilityTextDirection::kNone;
}

bool IsRightToLeftRun(const chrome_pdf::AccessibilityTextRunInfo& run) {
  return run.direction == chrome_pdf::AccessibilityTextDirection::kRightToLeft;
}

bool IsVerticalTextDirection(chrome_pdf::AccessibilityTextDirection direction) {
  return direction == chrome_pdf::AccessibilityTextDirection::kTopToBottom ||
         direction == chrome_pdf::AccessibilityTextDirection::kBottomToTop;
}

std::optional<int> ParseRomanNumeral(std::string_view text) {
  if (text.empty() || !(std::ranges::all_of(text, base::IsAsciiLower<char>) ||
                        std::ranges::all_of(text, base::IsAsciiUpper<char>))) {
    return std::nullopt;
  }
  const std::string lower_text = base::ToLowerASCII(text);

  // Up to `kMaxRomanNumeralValue`, a numeral is an "x" for each ten in the tens
  // place, followed by the ones place, e.g. "xx" + "iv" for 24.
  int tens_place_value = 0;
  std::string_view ones_place = lower_text;
  while (ones_place.starts_with('x')) {
    tens_place_value += 10;
    ones_place.remove_prefix(1);
  }
  const auto ones_place_entry = kRomanNumeralOnesPlace.find(ones_place);
  if (ones_place_entry == kRomanNumeralOnesPlace.end() ||
      tens_place_value + ones_place_entry->second > kMaxRomanNumeralValue) {
    return std::nullopt;
  }
  return tens_place_value + ones_place_entry->second;
}

}  // namespace pdf
