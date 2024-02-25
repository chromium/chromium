// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_browser_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace translate {

namespace {

// Constant string values to indicate UMA names. All entries should have
// a corresponding index in MetricsNameIndex and an entry in |kMetricsEntries|.
const char kTranslateLanguageDetectionContentLength[] =
    "Translate.LanguageDetection.ContentLength";
const char kTranslateHrefHintStatus[] = "Translate.HrefHint.Status";
const char kTranslateMenuTranslationUnavailableReasons[] =
    "Translate.MenuTranslation.UnavailableReasons";

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

}  // namespace TranslateBrowserMetrics

}  // namespace translate
