// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/ulp_metrics_logger.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(ULPMetricsLoggerTest, TestLanguageCount) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationLanguageCount(2);

  histogram.ExpectUniqueSample(kInitiationLanguageCountHistogram, 2, 1);
}

TEST(ULPMetricsLoggerTest, TestUILanguageStatus) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationUILanguageInULP(
      ULPLanguageStatus::kTopULPLanguageExactMatch);

  histogram.ExpectUniqueSample(kInitiationUILanguageInULPHistogram,
                               ULPLanguageStatus::kTopULPLanguageExactMatch, 1);
}

TEST(ULPMetricsLoggerTest, TestTranslateTargetStatus) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationTranslateTargetInULP(
      ULPLanguageStatus::kNonTopULPLanguageExactMatch);

  histogram.ExpectUniqueSample(kInitiationTranslateTargetInULPHistogram,
                               ULPLanguageStatus::kNonTopULPLanguageExactMatch,
                               1);
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

TEST(ULPMetricsLoggerTest, TestNeverLanguagesMissingFromULP) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  std::vector<std::string> never_languages_not_in_ulp = {"en-US", "de"};
  logger.RecordInitiationNeverLanguagesMissingFromULP(
      never_languages_not_in_ulp);

  histogram.ExpectBucketCount(kInitiationNeverLanguagesMissingFromULP,
                              base::HashMetricName("en-US"), 1);
  histogram.ExpectBucketCount(kInitiationNeverLanguagesMissingFromULP,
                              base::HashMetricName("de"), 1);
}

TEST(ULPMetricsLoggerTest, TestNeverLanguagesMissingFromULPCount) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationNeverLanguagesMissingFromULPCount(3);
  histogram.ExpectUniqueSample(kInitiationNeverLanguagesMissingFromULPCount, 3,
                               1);
}

TEST(ULPMetricsLoggerTest, TestAcceptLanguagesPageLanguageOverlap) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationAcceptLanguagesPageLanguageOverlap(30);
  histogram.ExpectUniqueSample(
      kInitiationAcceptLanguagesPageLanguageOverlapHistogram, 30, 1);
}

TEST(ULPMetricsLoggerTest, TestPageLanguagesMissingFromULP) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  std::vector<std::string> page_languages_not_in_ulp = {"en-GB", "sw"};
  logger.RecordInitiationPageLanguagesMissingFromULP(page_languages_not_in_ulp);
  histogram.ExpectBucketCount(kInitiationPageLanguagesMissingFromULPHistogram,
                              base::HashMetricName("en-GB"), 1);
  histogram.ExpectBucketCount(kInitiationPageLanguagesMissingFromULPHistogram,
                              base::HashMetricName("sw"), 1);
}

TEST(ULPMetricsLoggerTest, TestPageLanguagesMissingFromULPCount) {
  ULPMetricsLogger logger;
  base::HistogramTester histogram;

  logger.RecordInitiationPageLanguagesMissingFromULPCount(2);
  histogram.ExpectUniqueSample(
      kInitiationPageLanguagesMissingFromULPCountHistogram, 2, 1);
}

TEST(ULPMetricsLoggerTest, TestDetermineLanguageStatus) {
  std::vector<std::string> ulp_languages = {"en-US", "es-419", "pt-BR", "de",
                                            "fr-CA"};

  EXPECT_EQ(ULPLanguageStatus::kTopULPLanguageExactMatch,
            ULPMetricsLogger::DetermineLanguageStatus("en-US", ulp_languages));

  EXPECT_EQ(ULPLanguageStatus::kNonTopULPLanguageExactMatch,
            ULPMetricsLogger::DetermineLanguageStatus("de", ulp_languages));

  EXPECT_EQ(ULPLanguageStatus::kTopULPLanguageBaseMatch,
            ULPMetricsLogger::DetermineLanguageStatus("en-GB", ulp_languages));

  EXPECT_EQ(ULPLanguageStatus::kNonTopULPLanguageBaseMatch,
            ULPMetricsLogger::DetermineLanguageStatus("pt", ulp_languages));

  EXPECT_EQ(ULPLanguageStatus::kLanguageNotInULP,
            ULPMetricsLogger::DetermineLanguageStatus("zu", ulp_languages));

  EXPECT_EQ(ULPLanguageStatus::kLanguageEmpty,
            ULPMetricsLogger::DetermineLanguageStatus("", ulp_languages));

  EXPECT_EQ(ULPLanguageStatus::kLanguageEmpty,
            ULPMetricsLogger::DetermineLanguageStatus("und", ulp_languages));
}

TEST(ULPMetricsLoggerTest, TestULPLanguagesOverlapRatio) {
  std::vector<std::string> languages = {"en-US", "es", "pt-BR", "de", "fr-CA"};

  EXPECT_EQ(0, ULPMetricsLogger::LanguagesOverlapRatio(languages,
                                                       {"fi-FI", "af", "zu"}));

  EXPECT_EQ(20, ULPMetricsLogger::LanguagesOverlapRatio(languages,
                                                        {"en-GB", "af", "zu"}));

  EXPECT_EQ(20, ULPMetricsLogger::LanguagesOverlapRatio(languages,
                                                        {"en", "af", "zu"}));

  EXPECT_EQ(40, ULPMetricsLogger::LanguagesOverlapRatio(
                    languages, {"en-US", "af", "zu", "es"}));

  EXPECT_EQ(60, ULPMetricsLogger::LanguagesOverlapRatio(
                    languages, {"en-US", "af", "pt-BR", "es"}));

  EXPECT_EQ(60, ULPMetricsLogger::LanguagesOverlapRatio(
                    languages, {"en", "af", "pt", "es"}));

  EXPECT_EQ(60, ULPMetricsLogger::LanguagesOverlapRatio(
                    languages, {"en", "af", "pt-PT", "es"}));

  EXPECT_EQ(80, ULPMetricsLogger::LanguagesOverlapRatio(
                    languages, {"en-US", "af", "pt-BR", "es", "de"}));

  EXPECT_EQ(100, ULPMetricsLogger::LanguagesOverlapRatio(
                     languages, {"en-US", "af", "pt-BR", "es", "de", "fr-CA"}));
}

TEST(ULPMetricsLoggerTest, TestRemoveULPLanguages) {
  std::vector<std::string> ulp_languages = {"en-US", "es", "pt-BR", "de"};

  EXPECT_THAT(ULPMetricsLogger::RemoveULPLanguages({"af", "en", "am", "as"},
                                                   ulp_languages),
              ElementsAre("af", "am", "as"));

  EXPECT_THAT(ULPMetricsLogger::RemoveULPLanguages(
                  {"en-GB", "af", "en-AU", "am", "pt", "as"}, ulp_languages),
              ElementsAre("af", "am", "as"));

  EXPECT_THAT(ULPMetricsLogger::RemoveULPLanguages({"en", "pt-BR", "es-MX"},
                                                   ulp_languages),
              IsEmpty());
}

}  // namespace language
