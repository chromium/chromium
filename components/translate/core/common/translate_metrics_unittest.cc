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

  void CheckLanguageVerification(int expected_model_only,
                                 int expected_unknown,
                                 int expected_model_agree,
                                 int expected_model_disagree,
                                 int expected_trust_model,
                                 int expected_model_complement_sub_code,
                                 int expected_no_page_content,
                                 int expected_model_not_available) {
    ASSERT_EQ(metrics_internal::kTranslateLanguageDetectionLanguageVerification,
              key_);

    Snapshot();

    // EXPECT_EQ(expected_model_disabled,
    //           GetCountWithoutSnapshot(kModelDisabled)); -- obsolete
    EXPECT_EQ(expected_model_only, GetCountWithoutSnapshot(static_cast<int>(
                                       LanguageVerificationType::kModelOnly)));
    EXPECT_EQ(expected_unknown, GetCountWithoutSnapshot(static_cast<int>(
                                    LanguageVerificationType::kModelUnknown)));
    EXPECT_EQ(expected_model_agree,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kModelAgrees)));
    EXPECT_EQ(expected_model_disagree,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kModelDisagrees)));
    EXPECT_EQ(expected_trust_model,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kModelOverrides)));
    EXPECT_EQ(expected_model_complement_sub_code,
              GetCountWithoutSnapshot(static_cast<int>(
                  LanguageVerificationType::kModelComplementsCountry)));
    EXPECT_EQ(expected_no_page_content,
              GetCountWithoutSnapshot(
                  static_cast<int>(LanguageVerificationType::kNoPageContent)));
    EXPECT_EQ(expected_model_not_available,
              GetCountWithoutSnapshot(static_cast<int>(
                  LanguageVerificationType::kModelNotAvailable)));
  }

  void CheckTotalCount(int count) {
    Snapshot();
    EXPECT_EQ(count, GetTotalCount());
  }

  void CheckCount(HistogramBase::Sample value, int expected) {
    if (!samples_) {
      Snapshot();
    }
    EXPECT_EQ(expected, GetCountWithoutSnapshot(value));
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

  // ReportLanguageVerification(kModelDisabled); -- obsolete
  recorder.CheckLanguageVerification(0, 0, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelOnly);
  recorder.CheckLanguageVerification(1, 0, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelUnknown);
  recorder.CheckLanguageVerification(1, 1, 0, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelAgrees);
  recorder.CheckLanguageVerification(1, 1, 1, 0, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelDisagrees);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 0, 0, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelOverrides);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 0, 0, 0);
  ReportLanguageVerification(
      LanguageVerificationType::kModelComplementsCountry);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 0, 0);
  ReportLanguageVerification(LanguageVerificationType::kNoPageContent);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1, 0);
  ReportLanguageVerification(LanguageVerificationType::kModelNotAvailable);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1, 1);
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

TEST(TranslateMetricsTest, ReportCompactInfobarEvent) {
  MetricsRecorder recorder(metrics_internal::kTranslateCompactInfobarEvent);
  recorder.CheckTotalCount(0);
  ReportCompactInfobarEvent(InfobarEvent::INFOBAR_IMPRESSION);
  ReportCompactInfobarEvent(InfobarEvent::INFOBAR_IMPRESSION);
  ReportCompactInfobarEvent(InfobarEvent::INFOBAR_IMPRESSION);
  ReportCompactInfobarEvent(InfobarEvent::INFOBAR_REVERT);
  ReportCompactInfobarEvent(InfobarEvent::INFOBAR_REVERT);
  ReportCompactInfobarEvent(
      InfobarEvent::INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION);

  recorder.CheckTotalCount(6);
  recorder.CheckCount(static_cast<int>(InfobarEvent::INFOBAR_IMPRESSION), 3);
  recorder.CheckCount(static_cast<int>(InfobarEvent::INFOBAR_REVERT), 2);
  recorder.CheckCount(
      static_cast<int>(InfobarEvent::INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION),
      1);
}

}  // namespace
}  // namespace translate
