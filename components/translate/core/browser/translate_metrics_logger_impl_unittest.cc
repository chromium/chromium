// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class TranslateMetricsLoggerImplTest : public testing::Test {
 public:
  void SetUp() override {
    translate_metrics_logger_ =
        std::make_unique<translate::TranslateMetricsLoggerImpl>(
            nullptr /*translate_manager*/);

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  translate::TranslateMetricsLoggerImpl* translate_metrics_logger() {
    return translate_metrics_logger_.get();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  // Test target.
  std::unique_ptr<translate::TranslateMetricsLoggerImpl>
      translate_metrics_logger_;

  // Records the UMA histograms for each test.
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(TranslateMetricsLoggerImplTest, MultipleRecordMetrics) {
  // Set test constants and log them with the test target.
  translate::RankerDecision ranker_decision =
      translate::RankerDecision::kShowUI;
  uint32_t ranker_model_version = 1234;

  translate_metrics_logger()->LogRankerMetrics(ranker_decision,
                                               ranker_model_version);

  // Simulate |RecordMetrics| being called multiple times.
  translate_metrics_logger()->RecordMetrics(false);
  translate_metrics_logger()->RecordMetrics(false);
  translate_metrics_logger()->RecordMetrics(true);

  // The page-load UMA metrics should only be logged when the first
  // |RecordMetrics| is called. Subsequent calls shouldn't cause UMA metrics to
  // be logged.
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerDecision, ranker_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerVersion, ranker_model_version, 1);
}

TEST_F(TranslateMetricsLoggerImplTest, LogRankerMetrics) {
  translate::RankerDecision ranker_decision =
      translate::RankerDecision::kDontShowUI;
  uint32_t ranker_model_version = 4321;

  translate_metrics_logger()->LogRankerMetrics(ranker_decision,
                                               ranker_model_version);

  translate_metrics_logger()->RecordMetrics(true);

  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerDecision, ranker_decision, 1);
  histogram_tester()->ExpectUniqueSample(
      translate::kTranslatePageLoadRankerVersion, ranker_model_version, 1);
}
