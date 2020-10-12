// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_browser_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "components/language_usage_metrics/language_usage_metrics.h"

namespace translate {

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateInitiationStatus[] = "Translate.InitiationStatus.v2";
const char kTranslateReportLanguageDetectionError[] =
    "Translate.ReportLanguageDetectionError";
const char kTranslateLanguageDetectionContentLength[] =
    "Translate.LanguageDetection.ContentLength";
const char kTranslateLocalesOnDisabledByPrefs[] =
    "Translate.LocalesOnDisabledByPrefs";
const char kTranslateUndisplayableLanguage[] =
    "Translate.UndisplayableLanguage";
const char kTranslateUnsupportedLanguageAtInitiation[] =
    "Translate.UnsupportedLanguageAtInitiation";
const char kTranslateSourceLanguage[] = "Translate.SourceLanguage";
const char kTranslateTargetLanguage[] = "Translate.TargetLanguage";
const char kTranslateHrefHintStatus[] = "Translate.HrefHint.Status";
const char kTranslateHrefHintPrefsFilterStatus[] =
    "Translate.HrefHint.PrefsFilterStatus";
const char kTranslateTargetLanguageOrigin[] = "Translate.TargetLanguage.Origin";

}  // namespace

namespace TranslateBrowserMetrics {

void ReportInitiationStatus(InitiationStatusType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateInitiationStatus, type,
                            INITIATION_STATUS_MAX);
}

void ReportLanguageDetectionError() {
  UMA_HISTOGRAM_BOOLEAN(kTranslateReportLanguageDetectionError, true);
}

void ReportLanguageDetectionContentLength(size_t length) {
  base::UmaHistogramCounts100000(kTranslateLanguageDetectionContentLength,
                                 length);
}

void ReportLocalesOnDisabledByPrefs(base::StringPiece locale) {
  base::UmaHistogramSparse(
      kTranslateLocalesOnDisabledByPrefs,
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(locale));
}

void ReportUndisplayableLanguage(base::StringPiece language) {
  int language_code =
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(language);
  base::UmaHistogramSparse(kTranslateUndisplayableLanguage, language_code);
}

void ReportUnsupportedLanguageAtInitiation(base::StringPiece language) {
  int language_code =
      language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(language);
  base::UmaHistogramSparse(kTranslateUnsupportedLanguageAtInitiation,
                           language_code);
}

void ReportTranslateSourceLanguage(base::StringPiece language) {
  base::UmaHistogramSparse(kTranslateSourceLanguage,
                           base::HashMetricName(language));
}

void ReportTranslateTargetLanguage(base::StringPiece language) {
  base::UmaHistogramSparse(kTranslateTargetLanguage,
                           base::HashMetricName(language));
}

void ReportTranslateHrefHintStatus(HrefTranslateStatus status) {
  base::UmaHistogramEnumeration(kTranslateHrefHintStatus, status);
}

void ReportTranslateHrefHintPrefsFilterStatus(
    HrefTranslatePrefsFilterStatus status) {
  base::UmaHistogramEnumeration(kTranslateHrefHintPrefsFilterStatus, status);
}

void ReportTranslateTargetLanguageOrigin(TargetLanguageOrigin origin) {
  base::UmaHistogramEnumeration(kTranslateTargetLanguageOrigin, origin);
}

}  // namespace TranslateBrowserMetrics

}  // namespace translate
