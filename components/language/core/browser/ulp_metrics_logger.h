// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_ULP_METRICS_LOGGER_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_ULP_METRICS_LOGGER_H_

#include <string>
#include <vector>

namespace language {

const char kInitiationLanguageCountHistogram[] =
    "LanguageUsage.ULP.Initiation.Count";
const char kInitiationUILanguageInULPHistogram[] =
    "LanguageUsage.ULP.Initiation.ChromeUILanguageInULP";
const char kInitiationTranslateTargetInULPHistogram[] =
    "LanguageUsage.ULP.Initiation.TranslateTargetInULP";
const char kInitiationTopAcceptLanguageInULPHistogram[] =
    "LanguageUsage.ULP.Initiation.TopAcceptLanguageInULP";
const char kInitiationAcceptLanguagesULPOverlapHistogram[] =
    "LanguageUsage.ULP.Initiation.AcceptLanguagesULPOverlap";

// Keep up to date with ULPLanguageStatus in
// //tools/metrics/histograms/enums.xml.
enum class ULPLanguageStatus {
  kTopULPLanguageExactMatch = 0,
  kNonTopULPLanguageExactMatch = 1,
  kLanguageNotInULP = 2,
  kTopULPLanguageBaseMatch = 3,
  kNonTopULPLanguageBaseMatch = 4,
  kLanguageEmpty = 5,
  kMaxValue = kLanguageEmpty,
};

// ULPMetricsLogger abstracts the UMA histograms populated by the User Language
// Profile (ULP) integration.
class ULPMetricsLogger {
 public:
  ULPMetricsLogger() = default;
  virtual ~ULPMetricsLogger() = default;

  // Record the number of languages in the user's LanguageProfile at ULP init.
  virtual void RecordInitiationLanguageCount(int count);

  // Record whether the UI language exists in the user's LanguageProfile at
  // init.
  virtual void RecordInitiationUILanguageInULP(ULPLanguageStatus status);

  // Record whether the user's translate target language exists in their
  // LanguageProfile at init.
  virtual void RecordInitiationTranslateTargetInULP(ULPLanguageStatus status);

  // Record whether the user's top accept language exists in their
  // LanguageProfile at init.
  virtual void RecordInitiationTopAcceptLanguageInULP(ULPLanguageStatus status);

  // Record the ratio of ULP languages s in Accept Languages : total ULP
  // languages at init.
  virtual void RecordInitiationAcceptLanguagesULPOverlap(
      int overlap_ratio_percent);

  // Returns an enum that indicates whether `language` is present in
  // `ulp_languages` and, if so, whether it was the first entry.
  virtual ULPLanguageStatus DetermineLanguageStatus(
      const std::string& language,
      const std::vector<std::string>& ulp_languages);

  // Returns a number from 0-100 that indicates the ratio of ulp_languages that
  // are present in accept_languages.
  virtual int ULPLanguagesInAcceptLanguagesRatio(
      const std::vector<std::string> accept_languages,
      const std::vector<std::string> ulp_languages);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_ULP_METRICS_LOGGER_H_
