// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/ulp_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "ui/base/l10n/l10n_util.h"

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

void ULPMetricsLogger::RecordInitiationNeverLanguagesMissingFromULP(
    const std::vector<std::string>& never_languages) {
  for (const auto& language : never_languages) {
    base::UmaHistogramSparse(kInitiationNeverLanguagesMissingFromULP,
                             base::HashMetricName(language));
  }
}

void ULPMetricsLogger::RecordInitiationNeverLanguagesMissingFromULPCount(
    int count) {
  UMA_HISTOGRAM_COUNTS_100(kInitiationNeverLanguagesMissingFromULPCount, count);
}

ULPLanguageStatus ULPMetricsLogger::DetermineLanguageStatus(
    const std::string& language,
    const std::vector<std::string>& ulp_languages) {
  if (language.empty() || language.compare("und") == 0) {
    return ULPLanguageStatus::kLanguageEmpty;
  }

  // Search for exact match of language in ulp_languages (e.g. pt-BR != pt-MZ).
  std::vector<std::string>::const_iterator exact_match =
      std::find(ulp_languages.begin(), ulp_languages.end(), language);
  if (exact_match == ulp_languages.begin()) {
    return ULPLanguageStatus::kTopULPLanguageExactMatch;
  } else if (exact_match != ulp_languages.end()) {
    return ULPLanguageStatus::kNonTopULPLanguageExactMatch;
  }

  // Now search for a base language match (e.g pt-BR == pt-MZ).
  const std::string base_language = l10n_util::GetLanguage(language);
  std::vector<std::string>::const_iterator base_match = std::find_if(
      ulp_languages.begin(), ulp_languages.end(),
      [&base_language](const std::string& ulp_language) {
        return base_language.compare(l10n_util::GetLanguage(ulp_language)) == 0;
      });
  if (base_match == ulp_languages.begin()) {
    return ULPLanguageStatus::kTopULPLanguageBaseMatch;
  } else if (base_match != ulp_languages.end()) {
    return ULPLanguageStatus::kNonTopULPLanguageBaseMatch;
  }
  return ULPLanguageStatus::kLanguageNotInULP;
}

int ULPMetricsLogger::ULPLanguagesInAcceptLanguagesRatio(
    const std::vector<std::string> accept_languages,
    const std::vector<std::string> ulp_languages) {
  if (ulp_languages.size() <= 0) {
    return 0;
  }

  int num_ulp_languages_also_in_accept_languages = 0;
  for (const std::string& ulp_language : ulp_languages) {
    // Search for base matches of ulp_language in accept_languages (e.g. pt-BR
    // == pt-MZ).
    const std::string base_ulp_language = l10n_util::GetLanguage(ulp_language);
    std::vector<std::string>::const_iterator base_match =
        std::find_if(accept_languages.begin(), accept_languages.end(),
                     [&base_ulp_language](const std::string& accept_language) {
                       return base_ulp_language.compare(
                                  l10n_util::GetLanguage(accept_language)) == 0;
                     });
    if (base_match != accept_languages.end()) {
      ++num_ulp_languages_also_in_accept_languages;
    }
  }
  return (100 * num_ulp_languages_also_in_accept_languages) /
         ulp_languages.size();
}

std::vector<std::string> ULPMetricsLogger::RemoveULPLanguages(
    const std::vector<std::string> languages,
    const std::vector<std::string> ulp_languages) {
  std::vector<std::string> filtered_languages;

  for (const auto& language : languages) {
    // Only add languages that do not have a base match in ulp_languages.
    const std::string base_language = l10n_util::GetLanguage(language);
    std::vector<std::string>::const_iterator base_match = std::find_if(
        ulp_languages.begin(), ulp_languages.end(),
        [&base_language](const std::string& ulp_language) {
          return base_language.compare(l10n_util::GetLanguage(ulp_language)) ==
                 0;
        });
    if (base_match == ulp_languages.end()) {
      filtered_languages.push_back(language);
    }
  }
  return filtered_languages;
}
}  // namespace language
