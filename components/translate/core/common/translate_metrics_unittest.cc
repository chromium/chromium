// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_metrics.h"

#include <memory>

#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::SampleCountIterator;
using base::StatisticsRecorder;
using base::TimeTicks;

namespace translate {

namespace {

const int kTrue = 1;
const int kFalse = 0;

class MetricsRecorder {
 public:
  explicit MetricsRecorder(const char* key) : key_(key) {
    HistogramBase* histogram = StatisticsRecorder::FindHistogram(key_);
    if (histogram)
      base_samples_ = histogram->SnapshotSamples();
  }

  void CheckLanguageVerification(int expected_model_disabled,
                                 int expected_model_only,
                                 int expected_unknown,
                                 int expected_model_agree,
                                 int expected_model_disagree,
                                 int expected_trust_model,
                                 int expected_model_complement_sub_code) {
    ASSERT_EQ(translate::metrics_internal::
                  kTranslateLanguageDetectionLanguageVerification,
              key_);

    Snapshot();

    EXPECT_EQ(expected_model_disabled,
              GetCountWithoutSnapshot(
                  translate::DEPRECATED_LANGUAGE_VERIFICATION_MODEL_DISABLED));
    EXPECT_EQ(
        expected_model_only,
        GetCountWithoutSnapshot(translate::LANGUAGE_VERIFICATION_MODEL_ONLY));
    EXPECT_EQ(expected_unknown, GetCountWithoutSnapshot(
                                    translate::LANGUAGE_VERIFICATION_UNKNOWN));
    EXPECT_EQ(
        expected_model_agree,
        GetCountWithoutSnapshot(translate::LANGUAGE_VERIFICATION_MODEL_AGREE));
    EXPECT_EQ(expected_model_disagree,
              GetCountWithoutSnapshot(
                  translate::LANGUAGE_VERIFICATION_MODEL_DISAGREE));
    EXPECT_EQ(
        expected_trust_model,
        GetCountWithoutSnapshot(translate::LANGUAGE_VERIFICATION_TRUST_MODEL));
    EXPECT_EQ(expected_model_complement_sub_code,
              GetCountWithoutSnapshot(
                  translate::LANGUAGE_VERIFICATION_MODEL_COMPLEMENT_SUB_CODE));
  }

  void CheckScheme(int expected_http, int expected_https, int expected_others) {
    ASSERT_EQ(translate::metrics_internal::kTranslatePageScheme, key_);

    Snapshot();

    EXPECT_EQ(expected_http, GetCountWithoutSnapshot(translate::SCHEME_HTTP));
    EXPECT_EQ(expected_https, GetCountWithoutSnapshot(translate::SCHEME_HTTPS));
    EXPECT_EQ(expected_others,
              GetCountWithoutSnapshot(translate::SCHEME_OTHERS));
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

  DISALLOW_COPY_AND_ASSIGN(MetricsRecorder);
};

}  // namespace

TEST(TranslateMetricsTest, ReportLanguageVerification) {
  MetricsRecorder recorder(translate::metrics_internal::
                               kTranslateLanguageDetectionLanguageVerification);

  recorder.CheckLanguageVerification(0, 0, 0, 0, 0, 0, 0);
  translate::ReportLanguageVerification(
      translate::DEPRECATED_LANGUAGE_VERIFICATION_MODEL_DISABLED);
  recorder.CheckLanguageVerification(1, 0, 0, 0, 0, 0, 0);
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_MODEL_ONLY);
  recorder.CheckLanguageVerification(1, 1, 0, 0, 0, 0, 0);
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_UNKNOWN);
  recorder.CheckLanguageVerification(1, 1, 1, 0, 0, 0, 0);
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_MODEL_AGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 0, 0, 0);
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_MODEL_DISAGREE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 0, 0);
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_TRUST_MODEL);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 0);
  translate::ReportLanguageVerification(
      translate::LANGUAGE_VERIFICATION_MODEL_COMPLEMENT_SUB_CODE);
  recorder.CheckLanguageVerification(1, 1, 1, 1, 1, 1, 1);
}

TEST(TranslateMetricsTest, ReportTimeToBeReady) {
  MetricsRecorder recorder(
      translate::metrics_internal::kTranslateTimeToBeReady);
  recorder.CheckTotalCount(0);
  translate::ReportTimeToBeReady(3.14);
  recorder.CheckValueInLogs(3.14);
  recorder.CheckTotalCount(1);
}

TEST(TranslateMetricsTest, ReportTimeToLoad) {
  MetricsRecorder recorder(translate::metrics_internal::kTranslateTimeToLoad);
  recorder.CheckTotalCount(0);
  translate::ReportTimeToLoad(573.0);
  recorder.CheckValueInLogs(573.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateMetricsTest, ReportTimeToTranslate) {
  MetricsRecorder recorder(
      translate::metrics_internal::kTranslateTimeToTranslate);
  recorder.CheckTotalCount(0);
  translate::ReportTimeToTranslate(4649.0);
  recorder.CheckValueInLogs(4649.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateMetricsTest, ReportUserActionDuration) {
  MetricsRecorder recorder(
      translate::metrics_internal::kTranslateUserActionDuration);
  recorder.CheckTotalCount(0);
  TimeTicks begin = TimeTicks::Now();
  TimeTicks end = begin + base::TimeDelta::FromSeconds(3776);
  translate::ReportUserActionDuration(begin, end);
  recorder.CheckValueInLogs(3776000.0);
  recorder.CheckTotalCount(1);
}

TEST(TranslateMetricsTest, ReportPageScheme) {
  MetricsRecorder recorder(translate::metrics_internal::kTranslatePageScheme);
  recorder.CheckScheme(0, 0, 0);
  translate::ReportPageScheme("http");
  recorder.CheckScheme(1, 0, 0);
  translate::ReportPageScheme("https");
  recorder.CheckScheme(1, 1, 0);
  translate::ReportPageScheme("ftp");
  recorder.CheckScheme(1, 1, 1);
}

TEST(TranslateMetricsTest, ReportSimilarLanguageMatch) {
  MetricsRecorder recorder(
      translate::metrics_internal::kTranslateSimilarLanguageMatch);
  recorder.CheckTotalCount(0);
  EXPECT_EQ(0, recorder.GetCount(kTrue));
  EXPECT_EQ(0, recorder.GetCount(kFalse));
  translate::ReportSimilarLanguageMatch(true);
  EXPECT_EQ(1, recorder.GetCount(kTrue));
  EXPECT_EQ(0, recorder.GetCount(kFalse));
  translate::ReportSimilarLanguageMatch(false);
  EXPECT_EQ(1, recorder.GetCount(kTrue));
  EXPECT_EQ(1, recorder.GetCount(kFalse));
}

TEST(TranslateMetricsTest, ReportTranslatedLanguageDetectionContentLength) {
  MetricsRecorder recorder(
      translate::metrics_internal::kTranslatedLanguageDetectionContentLength);
  recorder.CheckTotalCount(0);
  translate::ReportTranslatedLanguageDetectionContentLength(12345);
  recorder.CheckValueInLogs(12345);
  recorder.CheckTotalCount(1);
}

}  // namespace translate
