// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/locale_util.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

namespace {

// Pair of locales, where the first element should fallback to the second one.
struct LocaleUIFallbackPair {
  const char* const chosen_locale;
  const char* const fallback_locale;
};

// This list MUST be sorted by the first element in the pair, because we perform
// binary search on it.
// TODO(claudiomagni): Investigate Norvegian language. There are 2 codes ("nn",
// "no") that fallback to "nb", but the base language should be "nn".
const LocaleUIFallbackPair kLocaleUIFallbackTable[] = {
    {"en", "en-US"},     {"en-AU", "en-GB"},  {"en-CA", "en-GB"},
    {"en-IN", "en-GB"},  {"en-NZ", "en-GB"},  {"en-ZA", "en-GB"},
    {"es-AR", "es-419"}, {"es-CL", "es-419"}, {"es-CO", "es-419"},
    {"es-CR", "es-419"}, {"es-HN", "es-419"}, {"es-MX", "es-419"},
    {"es-PE", "es-419"}, {"es-US", "es-419"}, {"es-UY", "es-419"},
    {"es-VE", "es-419"}, {"it-CH", "it"},     {"nn", "nb"},
    {"no", "nb"},        {"pt", "pt-PT"}};

bool LocaleCompare(const LocaleUIFallbackPair& p1, const std::string& p2) {
  return p1.chosen_locale < p2;
}

bool GetUIFallbackLocale(const std::string& input, std::string* const output) {
  *output = input;
  const auto* it =
      std::lower_bound(std::begin(kLocaleUIFallbackTable),
                       std::end(kLocaleUIFallbackTable), input, LocaleCompare);
  if (it != std::end(kLocaleUIFallbackTable) && it->chosen_locale == input) {
    *output = it->fallback_locale;
    return true;
  }
  return false;
}

// Checks if |locale| is one of the actual locales supported as a UI languages.
bool IsAvailableUILocale(const std::string& locale) {
  const std::vector<std::string>& ui_locales = l10n_util::GetAvailableLocales();
  return std::find(ui_locales.begin(), ui_locales.end(), locale) !=
         ui_locales.end();
}

}  // namespace

void SplitIntoMainAndTail(const std::string& locale,
                          std::string* main_part,
                          std::string* tail_part) {
  DCHECK(main_part);
  DCHECK(tail_part);

  std::vector<base::StringPiece> chunks = base::SplitStringPiece(
      locale, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (chunks.empty())
    return;

  chunks[0].CopyToString(main_part);
  *tail_part = locale.substr(main_part->size());
}

std::string ExtractBaseLanguage(const std::string& language_code) {
  std::string base;
  std::string tail;
  SplitIntoMainAndTail(language_code, &base, &tail);
  return base;
}

bool ContainsSameBaseLanguage(const std::vector<std::string>& list,
                              const std::string& language_code) {
  const std::string base_language = ExtractBaseLanguage(language_code);
  for (const auto& item : list) {
    const std::string compare_base = ExtractBaseLanguage(item);
    if (compare_base == base_language)
      return true;
  }

  return false;
}

bool ConvertToFallbackUILocale(std::string* input_locale) {
  // 1) Convert input to a fallback, if available.
  std::string fallback;
  GetUIFallbackLocale(*input_locale, &fallback);

  // 2) Check if input is part of the UI languages.
  if (IsAvailableUILocale(fallback)) {
    *input_locale = fallback;
    return true;
  }

  return false;
}

bool ConvertToActualUILocale(std::string* input_locale) {
  if (ConvertToFallbackUILocale(input_locale))
    return true;

  // Check if the base language of the input is part of the UI languages.
  const std::string base = ExtractBaseLanguage(*input_locale);
  if (base != *input_locale && IsAvailableUILocale(base)) {
    *input_locale = base;
    return true;
  }

  return false;
}

}  // namespace language
