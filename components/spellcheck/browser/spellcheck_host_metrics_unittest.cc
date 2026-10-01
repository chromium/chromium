// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_host_metrics.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <string_view>

#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

class SpellcheckHostMetricsTest : public testing::Test {
 public:
  SpellcheckHostMetricsTest() {
  }

  void SetUp() override {
    metrics_ = std::make_unique<SpellCheckHostMetrics>();
  }

  SpellCheckHostMetrics* metrics() { return metrics_.get(); }
  void RecordWordCountsForTesting() { metrics_->RecordWordCounts(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<SpellCheckHostMetrics> metrics_;
};

TEST_F(SpellcheckHostMetricsTest, RecordEnabledStats) {
  const char kMetricName[] = "SpellCheck.Enabled2";
  base::HistogramTester histogram_tester1;

  metrics()->RecordEnabledStats(false);

  histogram_tester1.ExpectBucketCount(kMetricName, 0, 1);
  histogram_tester1.ExpectBucketCount(kMetricName, 1, 0);

  base::HistogramTester histogram_tester2;

  metrics()->RecordEnabledStats(true);

  histogram_tester2.ExpectBucketCount(kMetricName, 0, 0);
  histogram_tester2.ExpectBucketCount(kMetricName, 1, 1);
}

TEST_F(SpellcheckHostMetricsTest, RecordWordCountsDiscardsDuplicates) {
  // This test ensures that RecordWordCounts only records metrics if they
  // have changed from the last invocation.
  const auto histogram_names = std::to_array<const char*>({
      "SpellCheck.CheckedWords",
      "SpellCheck.MisspelledWords",
      "SpellCheck.ReplacedWords",
      "SpellCheck.ShownSuggestions",
  });

  // Ensure all histograms exist.
  metrics()->RecordCheckedWordStats(u"test", false);
  RecordWordCountsForTesting();

  // Create the tester, taking a snapshot of current histogram samples.
  base::HistogramTester histogram_tester;

  // Nothing changed, so this invocation should not affect any histograms.
  RecordWordCountsForTesting();

  // Get samples for all affected histograms.
  for (size_t i = 0; i < std::size(histogram_names); ++i)
    histogram_tester.ExpectTotalCount(histogram_names[i], 0);
}

TEST_F(SpellcheckHostMetricsTest, RecordSpellingServiceStats) {
  const char kMetricName[] = "SpellCheck.SpellingService.Enabled2";
  base::HistogramTester histogram_tester1;

  metrics()->RecordSpellingServiceStats(false);

  histogram_tester1.ExpectBucketCount(kMetricName, 0, 1);
  histogram_tester1.ExpectBucketCount(kMetricName, 1, 0);

  base::HistogramTester histogram_tester2;

  metrics()->RecordSpellingServiceStats(true);
  histogram_tester2.ExpectBucketCount(kMetricName, 0, 0);
  histogram_tester2.ExpectBucketCount(kMetricName, 1, 1);
}

#if BUILDFLAG(IS_WIN)
TEST_F(SpellcheckHostMetricsTest, RecordAcceptLanguageStats) {
  struct Expectation {
    std::string_view histogram_name;
    int expected_count;
  };
  constexpr std::array kExpectations = std::to_array<Expectation>({
      {"Spellcheck.Windows.ChromeLocalesSupport2.Both", 1},
      {"Spellcheck.Windows.ChromeLocalesSupport2.HunspellOnly", 2},
      {"Spellcheck.Windows.ChromeLocalesSupport2.NativeOnly", 3},
      {"Spellcheck.Windows.ChromeLocalesSupport2.NoSupport", 4},
  });
  base::HistogramTester histogram_tester;

  SpellCheckHostMetrics::RecordAcceptLanguageStats({
      kExpectations[0].expected_count,
      kExpectations[1].expected_count,
      kExpectations[2].expected_count,
      kExpectations[3].expected_count,
  });

  for (const auto& expectation : kExpectations) {
    histogram_tester.ExpectTotalCount(expectation.histogram_name, 1);
    histogram_tester.ExpectBucketCount(expectation.histogram_name,
                                       expectation.expected_count, 1);
  }
}

TEST_F(SpellcheckHostMetricsTest, RecordSpellcheckLanguageStats) {
  struct Expectation {
    std::string_view histogram_name;
    int expected_count;
  };
  constexpr std::array kExpectations = std::to_array<Expectation>({
      {"Spellcheck.Windows.SpellcheckLocalesSupport2.Both", 1},
      {"Spellcheck.Windows.SpellcheckLocalesSupport2.HunspellOnly", 2},
      {"Spellcheck.Windows.SpellcheckLocalesSupport2.NativeOnly", 3},
  });
  base::HistogramTester histogram_tester;

  SpellCheckHostMetrics::RecordSpellcheckLanguageStats({
      kExpectations[0].expected_count,
      kExpectations[1].expected_count,
      kExpectations[2].expected_count,
      0,
  });

  for (const auto& expectation : kExpectations) {
    histogram_tester.ExpectTotalCount(expectation.histogram_name, 1);
    histogram_tester.ExpectBucketCount(expectation.histogram_name,
                                       expectation.expected_count, 1);
  }
}
#endif  // BUILDFLAG(IS_WIN)
