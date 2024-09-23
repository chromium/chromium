// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/spellcheck/browser/spellcheck_host_metrics.h"

#include <stddef.h>

#include <memory>

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
  const char* const histogram_names[] = {
      "SpellCheck.CheckedWords", "SpellCheck.MisspelledWords",
      "SpellCheck.ReplacedWords", "SpellCheck.UniqueWords",
      "SpellCheck.ShownSuggestions"};

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
  const char* const histogram_names[] = {
      "Spellcheck.Windows.ChromeLocalesSupport2.Both",
      "Spellcheck.Windows.ChromeLocalesSupport2.HunspellOnly",
      "Spellcheck.Windows.ChromeLocalesSupport2.NativeOnly",
      "Spellcheck.Windows.ChromeLocalesSupport2.NoSupport"};
  const size_t expected_counts[] = {1, 2, 3, 4};
  base::HistogramTester histogram_tester;

  SpellCheckHostMetrics::RecordAcceptLanguageStats({
      expected_counts[0],
      expected_counts[1],
      expected_counts[2],
      expected_counts[3],
  });

  for (size_t i = 0; i < std::size(histogram_names); ++i) {
    histogram_tester.ExpectTotalCount(histogram_names[i], 1);
    histogram_tester.ExpectBucketCount(histogram_names[i],
                                       static_cast<int>(expected_counts[i]), 1);
  }
}

TEST_F(SpellcheckHostMetricsTest, RecordSpellcheckLanguageStats) {
  const char* const histogram_names[] = {
      "Spellcheck.Windows.SpellcheckLocalesSupport2.Both",
      "Spellcheck.Windows.SpellcheckLocalesSupport2.HunspellOnly",
      "Spellcheck.Windows.SpellcheckLocalesSupport2.NativeOnly"};
  const size_t expected_counts[] = {1, 2, 3};
  base::HistogramTester histogram_tester;

  SpellCheckHostMetrics::RecordSpellcheckLanguageStats({
      expected_counts[0],
      expected_counts[1],
      expected_counts[2],
      0,
  });

  for (size_t i = 0; i < std::size(histogram_names); ++i) {
    histogram_tester.ExpectTotalCount(histogram_names[i], 1);
    histogram_tester.ExpectBucketCount(histogram_names[i],
                                       static_cast<int>(expected_counts[i]), 1);
  }
}
#endif  // BUILDFLAG(IS_WIN)
