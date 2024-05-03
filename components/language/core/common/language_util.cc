// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_util.h"

#include <stddef.h>

#include <algorithm>
#include <string_view>

#include "base/strings/strcat.h"
#include "components/country_codes/country_codes.h"
#include "components/language/core/common/locale_util.h"

namespace language {

namespace {

struct LanguageCodePair {
  // Code used in supporting list of Translate.
  const char* const translate_language;

  // Code used in Chrome internal.
  const char* const chrome_language;
};

// Some languages are treated as same languages in Translate even though they
// are different to be exact.
//
// If this table is updated, please sync this with the synonym table in
// chrome/browser/resources/settings/languages_page/languages.ts.
const LanguageCodePair kTranslateOnlySynonyms[] = {
    {"no", "nb"},
    {"id", "in"},
};

// Some languages have changed codes over the years and sometimes the older
// codes are used, so we must see them as synonyms.
//
// If this table is updated, please sync this with the synonym table in
// chrome/browser/resources/settings/languages_page/languages.ts.
const LanguageCodePair kLanguageCodeSynonyms[] = {
    {"gom", "kok"},
    {"iw", "he"},
    {"jw", "jv"},
    {"tl", "fil"},
};

// Some Chinese language codes are compatible with zh-TW or zh-CN in terms of
// Translate.
//
// If this table is updated, please sync this with the synonym table in
// chrome/browser/resources/settings/languages_page/languages.ts.
const LanguageCodePair kLanguageCodeChineseCompatiblePairs[] = {
    {"zh-TW", "zh-HK"},
    {"zh-TW", "zh-MO"},
    {"zh-CN", "zh-SG"},
};

}  // namespace

bool OverrideTranslateTriggerInIndia() {
#if BUILDFLAG(IS_ANDROID)
  return country_codes::GetCurrentCountryCode() == "IN";
#else
  return false;
#endif
}

OverrideLanguageModel GetOverrideLanguageModel() {
  // Note: when there are multiple possible override models, the overrides
  // ordering is important as it allows us to have concurrent overrides in
  // experiment without having to partition them explicitly.
  if (OverrideTranslateTriggerInIndia()) {
    return OverrideLanguageModel::GEO;
  }

  return OverrideLanguageModel::DEFAULT;
}

void ToTranslateLanguageSynonym(std::string* language) {
  // Get the base language (e.g. "es" for "es-MX")
  std::string_view main_part = language::SplitIntoMainAndTail(*language).first;
  if (main_part.empty()) {
    return;
  }

  if (main_part == "mni") {
    // "mni-Mtei" does not have any mapping and as such we leave it as is.
    return;
  }

  if (main_part == "zh") {
    // Chinese is a special case, there can be two base languages: traditional
    // and simplified. The kLanguageCodeChineseCompatiblePairs list contains the
    // relation between various Chinese locales. We need to return the code from
    // that mapping - if it exists.
    for (const auto& language_pair : kLanguageCodeChineseCompatiblePairs) {
      if (*language == language_pair.chrome_language) {
        *language = language_pair.translate_language;
        return;
      }
    }
    // Note that "zh" does not have any mapping and as such we leave it as is.
    // See https://crbug/798512 for more info.
    return;
  }

  for (const auto& language_pair : kTranslateOnlySynonyms) {
    if (main_part == language_pair.chrome_language) {
      *language = language_pair.translate_language;
      return;
    }
  }

  // Apply linear search here because number of items in the list is just three.
  for (const auto& language_pair : kLanguageCodeSynonyms) {
    if (main_part == language_pair.chrome_language) {
      *language = language_pair.translate_language;
      return;
    }
  }

  // By default use the base language as the translate synonym.
  *language = std::string(main_part);
}

void ToChromeLanguageSynonym(std::string* language) {
  auto [main_part, tail_part] = language::SplitIntoMainAndTail(*language);
  if (main_part.empty()) {
    return;
  }

  // Apply linear search here because number of items in the list is just three.
  for (const auto& language_pair : kLanguageCodeSynonyms) {
    if (main_part == language_pair.translate_language) {
      main_part = language_pair.chrome_language;
      break;
    }
  }

  *language = base::StrCat({main_part, tail_part});
}

}  // namespace language
