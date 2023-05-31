// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_metrics.h"

#include <memory>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::SampleCountIterator;
using base::StatisticsRecorder;
using base::TimeTicks;

namespace translate {
namespace {

class MetricsRecorder {
 public:
  explicit MetricsRecorder(const char* key) : key_(key) {
    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (histogram)
      base_samples_ = histogram->SnapshotSamples();
  }

  MetricsRecorder(const MetricsRecorder&) = delete;
  MetricsRecorder& operator=(const MetricsRecorder&) = delete;

  void CheckLanguageVerification(int expected_model_disabled,
                                 int expected_model_only,
                                 int expected_unknown,
                                 int expected_model_agree,
                                 int expected_model_disagree,
                                 int expected_trust_model,
                                 int expected_model_complement_sub_code) {
    ASSERT_EQ(metrics_internal::kTranslateLanguageDetectionLanguageVerification,
              key_);

    Snapshot();

    EXPECT_EQ(expected_model_disabled,
              GetCountWithoutSnapshot(
                  DEPRECATED_LANGUAGE_VERIFICATION_MODEL_DISABLED));
    EXPECT_EQ(expected_model_only,
              GetCountWithoutSnapshot(LANGUAGE_VERIFICATION_MODEL_ONLY));
    EXPECT_EQ(expected_unknown,
              GetCountWithoutSnapshot(LANGUAGE_VERIFICATION_MODEL_UNKNOWN));
    EXPECT_EQ(expected_model_agree,
              GetCountWithoutSnapshot(LANGUAGE_VERIFICATION_MODEL_AGREES));
    EXPECT_EQ(expected_model_disagree,
              GetCountWithoutSnapshot(LANGUAGE_VERIFICATION_MODEL_DISAGREES));
    EXPECT_EQ(expected_trust_model,
              GetCountWithoutSnapshot(LANGUAGE_VERIFICATION_MODEL_OVERRIDES));
    EXPECT_EQ(expected_model_complement_sub_code,
              GetCountWithoutSnapshot(
                  LANGUAGE_VERIFICATION_MODEL_COMPLEMENTS_COUNTRY));
  }

  void CheckTotalCount(int count) {
    Snapshot();
    EXPECT_EQ(count, GetTotalCount());
  }

  void CheckValueInLogs(double value) {
    Snapshot();
    ASSERT_TRUE(samples_.get());
    for (std::unique_ptr<SampleCountIterator> i = samples_->Iterator();
         !i->Done(); i->Next()) {
      HistogramBase::Sample min;
      int64_t max;
      HistogramBase::Count count;
      i->Get(&min, &max, &count);
      if (min <= value && value <= max && count >= 1)
        return;
    }
    EXPECT_FALSE(true);
  }

  HistogramBase::Count GetCount(HistogramBase::Sample value) {
    Snapshot();
    return GetCountWithoutSnapshot(value);
  }

 private:
  void Snapshot() {
    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (!histogram)
      return;
    samples_ = histogram->SnapshotSamples();
  }

  HistogramBase::Count GetCountWithoutSnapshot(HistogramBase::Sample value) {
    if (!samples_)
      return 0;
    HistogramBase::Count count = samples_->GetCount(value);
    if (!base_samples_)
      return count;
    return count - base_samples_->GetCount(value);
  }

  HistogramBase::Count GetTotalCount() {
    if (!samples_)
      return 0;
    HistogramBase::Count count = samples_->TotalCount();
    if (!base_samples_)
      return count;
    return count - base_samples_->TotalCount();
  }

  std::string key_;
  std::unique_ptr<HistogramSamples> base_samples_;
  std::unique_ptr<HistogramSamples> samples_;
};

TEST(TranslateMetricsTest, ReportLanguageVerification) {
  MetricsRecorder recorder(
      metrics_internal::kTranslateLanguageDetectionLanguageVerification);

  recorder.CheckLanguageVerification(0, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(DEPRECATED_LANGUAGE_VERIFICATION_MODEL_DISABLED);
  recorder.CheckLanguageVerification(1, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LANGUAGE_VERIFICATION_MODEL_ONLY);
  recorder.CheckLanguageVerification(1, 1, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LANGUAGE_VERIFICATION_MODEL_UNKNOWN);
  recorder.CheckLanguageVerification(1, 1, 1, 0, 0, 0, 0);
  ReportLanguageVerification(LANGUAGE_VERIFICATION_MODEL_AGREES);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 0, 0, 0);
  ReportLanguageVerification(LANGUAGE_VERIFICATION_MODEL_DISAGREES);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 0, 0);
  ReportLanguageVerification(LANGUAGE_VERIFICATION_MODEL_OVERRIDES);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 0);
  ReportLanguageVerification(LANGUAGE_VERIFICATION_MODEL_COMPLEMENTS_COUNTRY);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1);
}

TEST(TranslateMetricsTest, ReportTimeToBeReady) {
  MetricsRecorder recorder(metrics_internal::kTranslateTimeToBeReady);
  recorder.CheckTotalCount(0);
  ReportTimeToBeReady(3.14);
  recorder.CheckValueInLogs(3.14);
  recorder.CheckTotalCount(1);
}

TEST(TranslateMetricsTest, ReportTimeToLoad) {
  MetricsRecorder recorder(metrics_internal::kTranslateTimeToLoad);
  recorder.CheckTotalCount(0);
  ReportTimeToLoad(573.0);
  recorder.CheckValueInLogs(573.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateMetricsTest, ReportTimeToTranslate) {
  MetricsRecorder recorder(metrics_internal::kTranslateTimeToTranslate);
  recorder.CheckTotalCount(0);
  ReportTimeToTranslate(4649.0);
  recorder.CheckValueInLogs(4649.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateMetricsTest, ReportTranslatedLanguageDetectionContentLength) {
  MetricsRecorder recorder(
      metrics_internal::kTranslatedLanguageDetectionContentLength);
  recorder.CheckTotalCount(0);
  ReportTranslatedLanguageDetectionContentLength(12345);
  recorder.CheckValueInLogs(12345);
  recorder.CheckTotalCount(1);
}

}  // namespace
}  // namespace translate
