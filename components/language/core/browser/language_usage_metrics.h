// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_USAGE_METRICS_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_USAGE_METRICS_H_

#include <set>
#include <string_view>

#include "base/gtest_prod_util.h"

namespace language {
class UrlLanguageHistogram;

// Methods to record language usage as UMA histograms.
class LanguageUsageMetrics {
 public:
  LanguageUsageMetrics() = delete;
  LanguageUsageMetrics(const LanguageUsageMetrics&) = delete;
  LanguageUsageMetrics& operator=(const LanguageUsageMetrics&) = delete;

  // Records accept languages as a UMA histogram. |accept_languages| is a
  // case-insensitive comma-separated list of languages/locales of either xx,
  // xx-YY, or xx_YY format where xx is iso-639 language code and YY is iso-3166
  // country code. Country code is ignored. That is, xx and XX-YY are considered
  // identical and recorded once.
  static void RecordAcceptLanguages(std::string_view accept_languages);

  // Records detected page language history as a UMA histogram.
  // |UrlLanguageHistogram| is a mapping of page language to frequency. Country
  // codes are ignored for page language. Each language is counted once
  // regardless of frequency. Languages with a frequency below 0.05 are ignored.
  static void RecordPageLanguages(
      const language::UrlLanguageHistogram& language_counts);

  // Maps |locale| to a hash value in the "LanguageName" enum.
  // Deprecated - please use the enum "LocaleCodeISO639" which maps the full
  // locale including country variant to a base::HashMetricName value.
  //
  // The language hash is calculated by splitting the locale on "-" and bit
  // shifting the char int values. For example, if |locale| is 'en' the codes
  // of 'e' and 'n' are 101 and 110 respectively, and the language hash will be
  // 101 * 256 + 100 = 25966. |locale| is case-insensitive and not checked for
  // validity. Returns 0 in case of errors.
  static int ToLanguageCodeHash(std::string_view locale);

 private:
  // Parses |accept_languages| and returns a set of language codes in
  // |languages|.
  static void ParseAcceptLanguages(std::string_view accept_languages,
                                   std::set<int>* languages);

  FRIEND_TEST_ALL_PREFIXES(LanguageUsageMetricsTest, ParseAcceptLanguages);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_USAGE_METRICS_H_
