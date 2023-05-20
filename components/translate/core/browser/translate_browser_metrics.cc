// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_browser_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "components/language/core/browser/language_usage_metrics.h"

namespace translate {

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateInitiationStatus[] = "Translate.InitiationStatus.v2";
const char kTranslateLanguageDetectionContentLength[] =
    "Translate.LanguageDetection.ContentLength";
const char kTranslateUnsupportedLanguageAtInitiation[] =
    "Translate.UnsupportedLanguageAtInitiation";
const char kTranslateHrefHintStatus[] = "Translate.HrefHint.Status";
const char kTranslateHrefHintPrefsFilterStatus[] =
    "Translate.HrefHint.PrefsFilterStatus";
const char kTranslateMenuTranslationUnavailableReasons[] =
    "Translate.MenuTranslation.UnavailableReasons";

}  // namespace

namespace TranslateBrowserMetrics {

void ReportInitiationStatus(InitiationStatusType type) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateInitiationStatus, type,
                            INITIATION_STATUS_MAX);
}

void ReportMenuTranslationUnavailableReason(
    MenuTranslationUnavailableReason reason) {
  UMA_HISTOGRAM_ENUMERATION(kTranslateMenuTranslationUnavailableReasons,
                            reason);
}

void ReportLanguageDetectionContentLength(size_t length) {
  base::UmaHistogramCounts100000(kTranslateLanguageDetectionContentLength,
                                 length);
}

void ReportUnsupportedLanguageAtInitiation(base::StringPiece language) {
  int language_code =
      language::LanguageUsageMetrics::ToLanguageCodeHash(language);
  base::UmaHistogramSparse(kTranslateUnsupportedLanguageAtInitiation,
                           language_code);
}

void ReportTranslateHrefHintStatus(HrefTranslateStatus status) {
  base::UmaHistogramEnumeration(kTranslateHrefHintStatus, status);
}

void ReportTranslateHrefHintPrefsFilterStatus(
    HrefTranslatePrefsFilterStatus status) {
  base::UmaHistogramEnumeration(kTranslateHrefHintPrefsFilterStatus, status);
}

}  // namespace TranslateBrowserMetrics

}  // namespace translate
