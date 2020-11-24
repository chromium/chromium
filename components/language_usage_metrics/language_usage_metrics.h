// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_USAGE_METRICS_LANGUAGE_USAGE_METRICS_H_
#define COMPONENTS_LANGUAGE_USAGE_METRICS_LANGUAGE_USAGE_METRICS_H_

#include <set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace language {
class UrlLanguageHistogram;
}

namespace language_usage_metrics {

// Methods to record language usage as UMA histograms.
class LanguageUsageMetrics {
 public:
  // Records accept languages as a UMA histogram. |accept_languages| is a
  // case-insensitive comma-separated list of languages/locales of either xx,
  // xx-YY, or xx_YY format where xx is iso-639 language code and YY is iso-3166
  // country code. Country code is ignored. That is, xx and XX-YY are considered
  // identical and recorded once.
  static void RecordAcceptLanguages(base::StringPiece accept_languages);

  // Records detected page language history as a UMA histogram.
  // |UrlLanguageHistogram| is a mapping of page language to frequency. Country
  // codes are ignored for page language. Each language is counted once
  // regardless of frequency. Languages with a frequency below 0.05 are ignored.
  static void RecordPageLanguages(
      const language::UrlLanguageHistogram& language_counts);

  // Records the application language as a UMA histogram. |application_locale|
  // is a case-insensitive locale string of either xx, xx-YY, or xx_YY format.
  // Only the language part (xx in the example) is considered.
  static void RecordApplicationLanguage(base::StringPiece application_locale);

  // Parses |locale| and returns the language code. Returns 0 in case of errors.
  // The language code is calculated from two alphabets. For example, if
  // |locale| is 'en' which represents 'English', the codes of 'e' and 'n' are
  // 101 and 110 respectively, and the language code will be 101 * 256 + 100 =
  // 25966.
  // |locale| is case-insensitive, such that "EN" will return the same language
  // code as "en". This function doesn't check whether |locale| is valid locale
  // or not.
  static int ToLanguageCode(base::StringPiece locale);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LanguageUsageMetrics);

  // Parses |accept_languages| and returns a set of language codes in
  // |languages|.
  static void ParseAcceptLanguages(base::StringPiece accept_languages,
                                   std::set<int>* languages);

  FRIEND_TEST_ALL_PREFIXES(LanguageUsageMetricsTest, ParseAcceptLanguages);
};

}  // namespace language_usage_metrics

#endif  // COMPONENTS_LANGUAGE_USAGE_METRICS_LANGUAGE_USAGE_METRICS_H_
