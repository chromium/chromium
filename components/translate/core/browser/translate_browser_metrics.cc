// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_browser_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"

namespace translate {

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateLanguageDetectionContentLength[] =
    "Translate.LanguageDetection.ContentLength";
const char kTranslateHrefHintStatus[] = "Translate.HrefHint.Status";
const char kTranslateMenuTranslationUnavailableReasons[] =
    "Translate.MenuTranslation.UnavailableReasons";
constexpr char kTranslatePdfSourceLanguage[] = "Translate.PDF.SourceLanguage";
constexpr char kTranslatePdfTargetLanguage[] = "Translate.PDF.TargetLanguage";

}  // namespace

namespace TranslateBrowserMetrics {

void ReportMenuTranslationUnavailableReason(
    MenuTranslationUnavailableReason reason) {
  base::UmaHistogramEnumeration(kTranslateMenuTranslationUnavailableReasons,
                                reason);
}

void ReportLanguageDetectionContentLength(size_t length) {
  base::UmaHistogramCounts100000(kTranslateLanguageDetectionContentLength,
                                 length);
}

void ReportTranslateHrefHintStatus(HrefTranslateStatus status) {
  base::UmaHistogramEnumeration(kTranslateHrefHintStatus, status);
}

void ReportPdfSourceLanguage(std::string_view language) {
  base::UmaHistogramSparse(kTranslatePdfSourceLanguage,
                           base::HashMetricName(language));
}

void ReportPdfTargetLanguage(std::string_view language) {
  base::UmaHistogramSparse(kTranslatePdfTargetLanguage,
                           base::HashMetricName(language));
}

}  // namespace TranslateBrowserMetrics

}  // namespace translate
