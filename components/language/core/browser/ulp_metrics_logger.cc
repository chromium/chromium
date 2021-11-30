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

}  // namespace language
