// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/translation_v2_utils.h"

#include <set>

#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {

namespace {

// Supported locales list of Translate v2 API.
//
// This list is manually pulled and crafted from
// https://cloud.google.com/translate/docs/languages.
//
// We use this hard-coded list for checking supported locales of Translate v2
// API. This list should be periodically updated if the list of the API changes.
//
// `TranslationV2Utils::IsSupported` only cares lang part of a locale. But this
// locale list can contain non-lang part as well. Our code process this list to
// do a check with lang parts.
//
// TODO(b/277757989): Add an optional automated test for translation v2 language
// list
const char* const kSupportedLocales[] = {
    "af", "ak",       "am",  "ar",    "as",    "ay",  "az", "be", "bg", "bho",
    "bm", "bn",       "bs",  "ca",    "ceb",   "ckb", "co", "cs", "cy", "da",
    "de", "doi",      "dv",  "ee",    "el",    "en",  "eo", "es", "et", "eu",
    "fa", "fi",       "fil", "fr",    "fy",    "ga",  "gd", "gl", "gn", "gom",
    "gu", "ha",       "haw", "he",    "hi",    "hmn", "hr", "ht", "hu", "hy",
    "id", "ig",       "ilo", "is",    "it",    "iw",  "ja", "jv", "jw", "ka",
    "kk", "km",       "kn",  "ko",    "kri",   "ku",  "ky", "la", "lb", "lg",
    "ln", "lo",       "lt",  "lus",   "lv",    "mai", "mg", "mi", "mk", "ml",
    "mn", "mni-Mtei", "mr",  "ms",    "mt",    "my",  "ne", "nl", "no", "nso",
    "ny", "om",       "or",  "pa",    "pl",    "ps",  "pt", "qu", "ro", "ru",
    "rw", "sa",       "sd",  "si",    "sk",    "sl",  "sm", "sn", "so", "sq",
    "sr", "st",       "su",  "sv",    "sw",    "ta",  "te", "tg", "th", "ti",
    "tk", "tl",       "tr",  "ts",    "tt",    "ug",  "uk", "ur", "uz", "vi",
    "xh", "yi",       "yo",  "zh-CN", "zh-TW", "zh",  "zu",
};

}  // namespace

// static
bool TranslationV2Utils::IsSupported(const std::string& language) {
  std::set<std::string> languages;
  for (const std::string& locale : kSupportedLocales) {
    languages.insert(l10n_util::GetLanguage(locale));
  }

  return languages.count(language) != 0;
}

}  // namespace quick_answers
