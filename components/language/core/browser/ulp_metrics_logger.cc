// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/ulp_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/ranges/algorithm.h"
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

void ULPMetricsLogger::RecordInitiationAcceptLanguagesPageLanguageOverlap(
    int overlap_ratio) {
  UMA_HISTOGRAM_PERCENTAGE(
      kInitiationAcceptLanguagesPageLanguageOverlapHistogram, overlap_ratio);
}
void ULPMetricsLogger::RecordInitiationPageLanguagesMissingFromULP(
    const std::vector<std::string>& page_languages) {
  for (const auto& language : page_languages) {
    base::UmaHistogramSparse(kInitiationPageLanguagesMissingFromULPHistogram,
                             base::HashMetricName(language));
  }
}
void ULPMetricsLogger::RecordInitiationPageLanguagesMissingFromULPCount(
    int count) {
  UMA_HISTOGRAM_COUNTS_100(kInitiationPageLanguagesMissingFromULPCountHistogram,
                           count);
}

ULPLanguageStatus ULPMetricsLogger::DetermineLanguageStatus(
    const std::string& language,
    const std::vector<std::string>& ulp_languages) {
  if (language.empty() || language.compare("und") == 0) {
    return ULPLanguageStatus::kLanguageEmpty;
  }

  // Search for exact match of language in ulp_languages (e.g. pt-BR != pt-MZ).
  std::vector<std::string>::const_iterator exact_match =
      base::ranges::find(ulp_languages, language);
  if (exact_match == ulp_languages.begin()) {
    return ULPLanguageStatus::kTopULPLanguageExactMatch;
  } else if (exact_match != ulp_languages.end()) {
    return ULPLanguageStatus::kNonTopULPLanguageExactMatch;
  }

  // Now search for a base language match (e.g pt-BR == pt-MZ).
  const std::string base_language = l10n_util::GetLanguage(language);
  std::vector<std::string>::const_iterator base_match = base::ranges::find_if(
      ulp_languages, [&base_language](const std::string& ulp_language) {
        return base_language.compare(l10n_util::GetLanguage(ulp_language)) == 0;
      });
  if (base_match == ulp_languages.begin()) {
    return ULPLanguageStatus::kTopULPLanguageBaseMatch;
  } else if (base_match != ulp_languages.end()) {
    return ULPLanguageStatus::kNonTopULPLanguageBaseMatch;
  }
  return ULPLanguageStatus::kLanguageNotInULP;
}

int ULPMetricsLogger::LanguagesOverlapRatio(
    const std::vector<std::string> languages,
    const std::vector<std::string> compare_languages) {
  if (languages.size() <= 0) {
    return 0;
  }

  int num_overlap_languages = 0;
  for (const std::string& language : languages) {
    // Search for base matches of language (e.g. pt-BR == pt-MZ).
    const std::string base_language = l10n_util::GetLanguage(language);
    if (base::ranges::any_of(
            compare_languages,
            [&base_language](const std::string& compare_language) {
              return base_language.compare(
                         l10n_util::GetLanguage(compare_language)) == 0;
            })) {
      ++num_overlap_languages;
    }
  }
  return (100 * num_overlap_languages) / languages.size();
}

std::vector<std::string> ULPMetricsLogger::RemoveULPLanguages(
    const std::vector<std::string> languages,
    const std::vector<std::string> ulp_languages) {
  std::vector<std::string> filtered_languages;

  for (const auto& language : languages) {
    // Only add languages that do not have a base match in ulp_languages.
    const std::string base_language = l10n_util::GetLanguage(language);
    if (base::ranges::none_of(
            ulp_languages, [&base_language](const std::string& ulp_language) {
              return base_language.compare(
                         l10n_util::GetLanguage(ulp_language)) == 0;
            })) {
      filtered_languages.push_back(language);
    }
  }
  return filtered_languages;
}
}  // namespace language
