// Copyright 2021 The Chromium Authors
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
    "LanguageUsage.ULP.Initiation.AcceptLanguagesULPOverlap.Base";
const char kInitiationNeverLanguagesMissingFromULP[] =
    "LanguageUsage.ULP.Initiation.NeverLanguagesMissingFromULP";
const char kInitiationNeverLanguagesMissingFromULPCount[] =
    "LanguageUsage.ULP.Initiation.NeverLanguagesMissingFromULP.Count";
const char kInitiationAcceptLanguagesPageLanguageOverlapHistogram[] =
    "LanguagesUsage.ULP.Initiation.AcceptLanguagesPageLanguageOverlap.Base";
const char kInitiationPageLanguagesMissingFromULPHistogram[] =
    "LanguagesUsage.ULP.Initiation.PageLanguagesMissingFromULP";
const char kInitiationPageLanguagesMissingFromULPCountHistogram[] =
    "LanguagesUsage.ULP.Initiation.PageLanguagesMissingFromULP.Count";

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

  // Record each Never Translate language that does not have a base match with a
  // ULP language.
  virtual void RecordInitiationNeverLanguagesMissingFromULP(
      const std::vector<std::string>& never_languages);

  // Record the count of Never Translate languages that do not have a base match
  // with a ULP language.
  virtual void RecordInitiationNeverLanguagesMissingFromULPCount(int count);

  virtual void RecordInitiationAcceptLanguagesPageLanguageOverlap(
      int overlap_ratio_percent);
  virtual void RecordInitiationPageLanguagesMissingFromULP(
      const std::vector<std::string>& page_languages);
  virtual void RecordInitiationPageLanguagesMissingFromULPCount(int count);

  // Returns an enum that indicates whether `language` is present in
  // `ulp_languages` and, if so, whether it was the first entry.
  static ULPLanguageStatus DetermineLanguageStatus(
      const std::string& language,
      const std::vector<std::string>& ulp_languages);

  // Returns a number from 0-100 that indicates the ratio of ulp_languages that
  // are present in accept_languages. Only language bases are compared (e.g
  // pt-BR == pt-MZ).
  static int LanguagesOverlapRatio(
      const std::vector<std::string> accept_languages,
      const std::vector<std::string> ulp_languages);

  // Returns a vector with languages that do not have a ULP base language match.
  static std::vector<std::string> RemoveULPLanguages(
      const std::vector<std::string> languages,
      const std::vector<std::string> ulp_languages);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_ULP_METRICS_LOGGER_H_
