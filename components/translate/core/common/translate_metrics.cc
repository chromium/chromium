// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_metrics.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "url/url_constants.h"

namespace translate {

namespace metrics_internal {

const char kTranslateLanguageDetectionLanguageVerification[] =
    "Translate.LanguageDetection.LanguageVerification";
const char kTranslateTimeToBeReady[] = "Translate.Translation.TimeToBeReady";
const char kTranslateTimeToLoad[] = "Translate.Translation.TimeToLoad";
const char kTranslateTimeToTranslate[] =
    "Translate.Translation.TimeToTranslate";
const char kTranslateLanguageDeterminedDuration[] =
    "Translate.LanguageDeterminedDuration";
const char kTranslatedLanguageDetectionContentLength[] =
    "Translate.Translation.LanguageDetection.ContentLength";

// Note: These string constants are repeated in TranslateCompactInfoBar.java.
const char kTranslateCompactInfobarEvent[] = "Translate.CompactInfobar.Event";

}  // namespace metrics_internal

void ReportLanguageVerification(LanguageVerificationType type) {
  base::UmaHistogramEnumeration(
      metrics_internal::kTranslateLanguageDetectionLanguageVerification, type,
      LanguageVerificationType::kMaxValue);
}

void ReportTimeToBeReady(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      metrics_internal::kTranslateTimeToBeReady,
      base::Microseconds(static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportTimeToLoad(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      metrics_internal::kTranslateTimeToLoad,
      base::Microseconds(static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportTimeToTranslate(double time_in_msec) {
  UMA_HISTOGRAM_MEDIUM_TIMES(
      metrics_internal::kTranslateTimeToTranslate,
      base::Microseconds(static_cast<int64_t>(time_in_msec * 1000.0)));
}

void ReportLanguageDeterminedDuration(base::TimeTicks begin,
                                      base::TimeTicks end) {
  if (begin.is_null()) {
    // For non-primary pages, `begin` wasn't set here as we returned without
    // doing anything in DidFinishNavigation for them. For prerendering pages,
    // `end` is also inaccurate as
    // translate::mojom::ContentTranslateDriver::RegisterPage call is deferred
    // by the capability control.
    return;
  }
  UMA_HISTOGRAM_LONG_TIMES(
      metrics_internal::kTranslateLanguageDeterminedDuration, end - begin);
}

void ReportTranslatedLanguageDetectionContentLength(size_t content_length) {
  base::UmaHistogramCounts100000(
      metrics_internal::kTranslatedLanguageDetectionContentLength,
      content_length);
}

void ReportCompactInfobarEvent(InfobarEvent event) {
  UMA_HISTOGRAM_ENUMERATION(metrics_internal::kTranslateCompactInfobarEvent,
                            event);
}

}  // namespace translate
