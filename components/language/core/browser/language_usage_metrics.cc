// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_usage_metrics.h"

#include <stddef.h>

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_tokenizer.h"
#include "components/language/core/browser/url_language_histogram.h"

namespace language {

// static
void LanguageUsageMetrics::RecordAcceptLanguages(
    std::string_view accept_languages) {
  std::set<int> languages;
  ParseAcceptLanguages(accept_languages, &languages);

  UMA_HISTOGRAM_COUNTS_100("LanguageUsage.AcceptLanguage.Count",
                           languages.size());
  for (int language_code : languages) {
    base::UmaHistogramSparse("LanguageUsage.AcceptLanguage", language_code);
  }
}

// static
void LanguageUsageMetrics::RecordPageLanguages(
    const language::UrlLanguageHistogram& language_counts) {
  const float kMinLanguageFrequency = 0.05;
  std::vector<language::UrlLanguageHistogram::LanguageInfo> top_languages =
      language_counts.GetTopLanguages();

  for (const language::UrlLanguageHistogram::LanguageInfo& language_info :
       top_languages) {
    if (language_info.frequency < kMinLanguageFrequency) {
      continue;
    }

    const int language_code = ToLanguageCodeHash(language_info.language_code);
    if (language_code != 0) {
      base::UmaHistogramSparse("LanguageUsage.MostFrequentPageLanguages",
                               language_code);
    }
  }
}

// static
int LanguageUsageMetrics::ToLanguageCodeHash(std::string_view locale) {
  std::string_view language_part =
      locale.substr(0U, locale.find_first_of("-_"));

  int language_code = 0;
  for (size_t i = 0U; i < language_part.size(); ++i) {
    // It's undefined behavior in C++ to left-shift a signed int past its sign
    // bit, so only shift until the int's sign bit is reached. Note that it's
    // safe to shift up to sizeof(int) times because each character is only
    // added if it's between 'a' and 'z', which all have a 0 in their 7th bit.
    // For example, for 4-byte ints, "zzzz" would be converted to 0x7A7A7A7A,
    // which doesn't quite reach the sign bit, making it safe to insert up to 4
    // characters.
    if (i == sizeof(language_code))
      return 0;

    char ch = language_part[i];
    if ('A' <= ch && ch <= 'Z')
      ch += ('a' - 'A');
    else if (ch < 'a' || 'z' < ch)
      return 0;

    language_code <<= 8;
    language_code += ch;
  }

  return language_code;
}

// static
void LanguageUsageMetrics::ParseAcceptLanguages(
    std::string_view accept_languages,
    std::set<int>* languages) {
  languages->clear();
  base::StringViewTokenizer locales(accept_languages, ",");
  while (locales.GetNext()) {
    const int language_code = ToLanguageCodeHash(locales.token_piece());
    if (language_code != 0)
      languages->insert(language_code);
  }
}

}  // namespace language
