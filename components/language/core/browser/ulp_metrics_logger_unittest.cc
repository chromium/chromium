// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/ulp_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

TEST(ULPMetricsLoggerTest, TestLanguageCount) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationLanguageCount(2);

  histogram.ExpectUniqueSample(kInitiationLanguageCountHistogram, 2, 1);
}

TEST(ULPMetricsLoggerTest, TestUILanguageStatus) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationUILanguageInULP(ULPLanguageStatus::kTopULPLanguage);

  histogram.ExpectUniqueSample(kInitiationUILanguageInULPHistogram,
                               ULPLanguageStatus::kTopULPLanguage, 1);
}

TEST(ULPMetricsLoggerTest, TestTranslateTargetStatus) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationTranslateTargetInULP(
      ULPLanguageStatus::kNonTopULPLanguage);

  histogram.ExpectUniqueSample(kInitiationTranslateTargetInULPHistogram,
                               ULPLanguageStatus::kNonTopULPLanguage, 1);
}

TEST(ULPMetricsLoggerTest, TestTopAcceptLanguageStatus) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationTopAcceptLanguageInULP(
      ULPLanguageStatus::kLanguageNotInULP);

  histogram.ExpectUniqueSample(kInitiationTopAcceptLanguageInULPHistogram,
                               ULPLanguageStatus::kLanguageNotInULP, 1);
}

TEST(ULPMetricsLoggerTest, TestAcceptLanguagesULPOverlap) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationAcceptLanguagesULPOverlap(21);

  histogram.ExpectUniqueSample(kInitiationAcceptLanguagesULPOverlapHistogram,
                               21, 1);
}

}  // namespace language
