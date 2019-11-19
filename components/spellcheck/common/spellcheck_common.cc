// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_common.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/urename.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace spellcheck {

struct LanguageRegion {
  const char* language;         // The language.
  const char* language_region;  // language & region, used by dictionaries.
};

struct LanguageVersion {
  const char* language;  // The language input.
  const char* version;   // The corresponding version.
};

static constexpr LanguageRegion kSupportedSpellCheckerLanguages[] = {
    // Several languages are not to be included in the spellchecker list:
    // th-TH, vi-VI.
    {"af", "af-ZA"},
    {"bg", "bg-BG"},
    {"ca", "ca-ES"},
    {"cs", "cs-CZ"},
    {"cy", "cy-GB"},
    {"da", "da-DK"},
    {"de", "de-DE"},
    {"el", "el-GR"},
    {"en-AU", "en-AU"},
    {"en-CA", "en-CA"},
    {"en-GB", "en-GB"},
    {"en-US", "en-US"},
    {"es", "es-ES"},
    {"es-419", "es-ES"},
    {"es-AR", "es-ES"},
    {"es-ES", "es-ES"},
    {"es-MX", "es-ES"},
    {"es-US", "es-ES"},
    {"et", "et-EE"},
    {"fa", "fa-IR"},
    {"fo", "fo-FO"},
    {"fr", "fr-FR"},
    {"he", "he-IL"},
    {"hi", "hi-IN"},
    {"hr", "hr-HR"},
    {"hu", "hu-HU"},
    {"hy", "hy"},
    {"id", "id-ID"},
    {"it", "it-IT"},
    {"ko", "ko"},
    {"lt", "lt-LT"},
    {"lv", "lv-LV"},
    {"nb", "nb-NO"},
    {"nl", "nl-NL"},
    {"pl", "pl-PL"},
    {"pt-BR", "pt-BR"},
    {"pt-PT", "pt-PT"},
    {"ro", "ro-RO"},
    {"ru", "ru-RU"},
    {"sh", "sh"},
    {"sk", "sk-SK"},
    {"sl", "sl-SI"},
    {"sq", "sq"},
    {"sr", "sr"},
    {"sv", "sv-SE"},
    {"ta", "ta-IN"},
    {"tg", "tg-TG"},
    {"tr", "tr-TR"},
    {"uk", "uk-UA"},
    {"vi", "vi-VN"},
};

bool IsValidRegion(const std::string& region) {
  for (const auto& lang_region : kSupportedSpellCheckerLanguages) {
    if (lang_region.language_region == region)
      return true;
  }
  return false;
}

// This function returns the language-region version of language name.
// e.g. returns hi-IN for hi.
std::string GetSpellCheckLanguageRegion(base::StringPiece input_language) {
  for (const auto& lang_region : kSupportedSpellCheckerLanguages) {
    if (lang_region.language == input_language)
      return lang_region.language_region;
  }

  return input_language.as_string();
}

base::FilePath GetVersionedFileName(base::StringPiece input_language,
                                    const base::FilePath& dict_dir) {
  // The default dictionary version is 3-0. This version indicates that the bdic
  // file contains a checksum.
  static const char kDefaultVersionString[] = "-3-0";

  // Add non-default version strings here. Use the same version for all the
  // dictionaries that you add at the same time. Increment the major version
  // number if you're updating either dic or aff files. Increment the minor
  // version number if you're updating only dic_delta files.
  static constexpr LanguageVersion kSpecialVersionString[] = {
      // Jan 9, 2013: Add "FLAG num" to aff to avoid heapcheck crash.
      {"tr-TR", "-4-0"},

      // Mar 4, 2014: Add Tajik dictionary.
      {"tg-TG", "-5-0"},

      // October 2017: Update from upstream.
      {"en-AU", "-8-0"},
      {"en-CA", "-8-0"},
      {"en-GB", "-8-0"},
      {"en-US", "-8-0"},

      // Feb 2019: Initial check-in of Welsh.
      {"cy-GB", "-1-0"},

      // April 2019: Initial check-in of Armenian.
      {"hy", "-1-0"},

      // April 2019: Update Persian
      {"fa-IR", "-8-0"},

      // November 2019: Update Serbian-Latin and Serbian-Cyrillic
      {"sh", "-4-0"},
      {"sr", "-4-0"},
  };

  // Generate the bdict file name using default version string or special
  // version string, depending on the language.
  std::string language = GetSpellCheckLanguageRegion(input_language);
  std::string version = kDefaultVersionString;
  for (const auto& lang_ver : kSpecialVersionString) {
    if (language == lang_ver.language) {
      version = lang_ver.version;
      break;
    }
  }
  std::string versioned_bdict_file_name(language + version + ".bdic");
  return dict_dir.AppendASCII(versioned_bdict_file_name);
}

std::string GetCorrespondingSpellCheckLanguage(base::StringPiece language) {
  std::string best_match;
  // Look for exact match in the Spell Check language list.
  for (const auto& lang_region : kSupportedSpellCheckerLanguages) {
    // First look for exact match in the language region of the list.
    if (lang_region.language == language)
      return language.as_string();

    // Next, look for exact match in the language_region part of the list.
    if (lang_region.language_region == language) {
      if (best_match.empty())
        best_match = lang_region.language;
    }
  }

  // No match found - return best match, if any.
  return best_match;
}

std::vector<std::string> SpellCheckLanguages() {
  std::vector<std::string> languages;
  for (const auto& lang_region : kSupportedSpellCheckerLanguages)
    languages.push_back(lang_region.language);
  return languages;
}

void GetISOLanguageCountryCodeFromLocale(const std::string& locale,
                                         std::string* language_code,
                                         std::string* country_code) {
  DCHECK(language_code);
  DCHECK(country_code);
  char language[ULOC_LANG_CAPACITY] = ULOC_ENGLISH;
  const char* country = "USA";
  if (!locale.empty()) {
    UErrorCode error = U_ZERO_ERROR;
    char id[ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY];
    uloc_addLikelySubtags(locale.c_str(), id, base::size(id), &error);
    error = U_ZERO_ERROR;
    uloc_getLanguage(id, language, base::size(language), &error);
    country = uloc_getISO3Country(id);
  }
  *language_code = std::string(language);
  *country_code = std::string(country);
}

}  // namespace spellcheck
