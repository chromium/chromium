// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TEXT_UTILS_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TEXT_UTILS_H_

#include <optional>
#include <string_view>

namespace chrome_pdf {
enum class AccessibilityTextDirection;
struct AccessibilityCharInfo;
struct AccessibilityTextRunInfo;
}  // namespace chrome_pdf

namespace pdf {

// The largest value `ParseRomanNumeral()` accepts. Values from 40 ("XL") up
// aren't supported.
//
// Larger values need "L", "C", "D" or "M", which also spell words and
// abbreviations, e.g. "mix", "civ." or "CD".
inline constexpr int kMaxRomanNumeralValue = 39;  // "XXXIX"

// Returns whether `char_info` is a Unicode whitespace character.
bool IsWhitespace(const chrome_pdf::AccessibilityCharInfo& char_info);

// Returns whether `run` is horizontal left-to-right text.
bool IsLeftToRightRun(const chrome_pdf::AccessibilityTextRunInfo& run);

// Returns whether `run` is horizontal right-to-left text.
bool IsRightToLeftRun(const chrome_pdf::AccessibilityTextRunInfo& run);

// Returns whether `direction` represents vertical text.
bool IsVerticalTextDirection(chrome_pdf::AccessibilityTextDirection direction);

// Returns the value of `text` as a Roman numeral from 1 to
// `kMaxRomanNumeralValue`, or `std::nullopt` if it isn't one.
//
// Only standard forms in a single case are accepted, e.g. "iv" or "IV" but not
// "iiii", "IIII" or "iV".
std::optional<int> ParseRomanNumeral(std::string_view text);

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TEXT_UTILS_H_
