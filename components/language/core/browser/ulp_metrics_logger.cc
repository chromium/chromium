// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/ulp_metrics_logger.h"

#include "base/metrics/histogram_macros.h"

namespace language {

void ULPMetricsLogger::RecordInitiationLanguageCount(int count) {
  UMA_HISTOGRAM_COUNTS_100(kInitiationLanguageCountHistogram, count);
}

void ULPMetricsLogger::RecordInitiationUILanguageInULP(
    ULPLanguageStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kInitiationUILanguageInULPHistogram, status);
}

void ULPMetricsLogger::RecordInitiationTranslateTargetInULP(
    ULPLanguageStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kInitiationTranslateTargetInULPHistogram, status);
}

void ULPMetricsLogger::RecordInitiationTopAcceptLanguageInULP(
    ULPLanguageStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kInitiationTopAcceptLanguageInULPHistogram, status);
}

void ULPMetricsLogger::RecordInitiationAcceptLanguagesULPOverlap(
    int overlap_ratio_percent) {
  UMA_HISTOGRAM_PERCENTAGE(kInitiationAcceptLanguagesULPOverlapHistogram,
                           overlap_ratio_percent);
}

ULPLanguageStatus ULPMetricsLogger::DetermineLanguageStatus(
    const std::string& language,
    const std::vector<std::string>& ulp_languages) {
  std::vector<std::string>::const_iterator i =
      std::find(ulp_languages.begin(), ulp_languages.end(), language);
  if (i == ulp_languages.end()) {
    return ULPLanguageStatus::kLanguageNotInULP;
  } else if (i == ulp_languages.begin()) {
    return ULPLanguageStatus::kTopULPLanguage;
  } else {
    return ULPLanguageStatus::kNonTopULPLanguage;
  }
}

int ULPMetricsLogger::ULPLanguagesInAcceptLanguagesRatio(
    const std::vector<std::string> accept_languages,
    const std::vector<std::string> ulp_languages) {
  if (ulp_languages.size() <= 0) {
    return 0;
  }

  int num_ulp_languages_also_in_accept_languages = 0;
  for (const std::string& ulp_language : ulp_languages) {
    if (std::find(accept_languages.begin(), accept_languages.end(),
                  ulp_language) != accept_languages.end()) {
      ++num_ulp_languages_also_in_accept_languages;
    }
  }
  return (100 * num_ulp_languages_also_in_accept_languages) /
         ulp_languages.size();
}
}  // namespace language
