// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_browser_metrics.h"

#include <memory>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;

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

  void CheckTranslateHrefHintStatus(
      int expected_auto_translated,
      int expected_auto_translated_different_target_language,
      int expected_ui_shown_not_auto_translated,
      int expected_no_ui_shown_not_auto_translated) {
    Snapshot();

    EXPECT_EQ(
        expected_auto_translated,
        GetCountWithoutSnapshot(static_cast<int>(
            TranslateBrowserMetrics::HrefTranslateStatus::kAutoTranslated)));
    EXPECT_EQ(expected_auto_translated_different_target_language,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::HrefTranslateStatus::
                      kAutoTranslatedDifferentTargetLanguage)));
    EXPECT_EQ(expected_ui_shown_not_auto_translated,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::HrefTranslateStatus::
                      kUiShownNotAutoTranslated)));
    EXPECT_EQ(expected_no_ui_shown_not_auto_translated,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::HrefTranslateStatus::
                      kNoUiShownNotAutoTranslated)));
  }

  void CheckMenuTranslationUnavailableReason(
      int expected_kTranslate_disabled,
      int expected_network_offline,
      int expected_api_keys_missing,
      int expected_unsupported_mimetype_page,
      int expected_url_not_translatable,
      int expected_target_lang_unknown,
      int expected_not_allowed_by_policy,
      int expected_source_lang_unknown) {
    Snapshot();

    EXPECT_EQ(expected_kTranslate_disabled,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kTranslateDisabled)));
    EXPECT_EQ(expected_network_offline,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kNetworkOffline)));
    EXPECT_EQ(expected_api_keys_missing,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kApiKeysMissing)));
    EXPECT_EQ(expected_unsupported_mimetype_page,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kMIMETypeUnsupported)));
    EXPECT_EQ(expected_url_not_translatable,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kURLNotTranslatable)));
    EXPECT_EQ(expected_target_lang_unknown,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kTargetLangUnknown)));
    EXPECT_EQ(expected_not_allowed_by_policy,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kNotAllowedByPolicy)));
    EXPECT_EQ(expected_source_lang_unknown,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::MenuTranslationUnavailableReason::
                      kSourceLangUnknown)));
  }

  HistogramBase::Count GetTotalCount() {
    Snapshot();
    if (!samples_)
      return 0;
    HistogramBase::Count count = samples_->TotalCount();
    if (!base_samples_)
      return count;
    return count - base_samples_->TotalCount();
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

  std::string key_;
  std::unique_ptr<HistogramSamples> base_samples_;
  std::unique_ptr<HistogramSamples> samples_;
};

TEST(TranslateBrowserMetricsTest, ReportMenuTranslationUnavailableReason) {
  MetricsRecorder recorder("Translate.MenuTranslation.UnavailableReasons");
  recorder.CheckMenuTranslationUnavailableReason(0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kTranslateDisabled);
  recorder.CheckMenuTranslationUnavailableReason(1, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kNetworkOffline);
  recorder.CheckMenuTranslationUnavailableReason(1, 1, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kApiKeysMissing);
  recorder.CheckMenuTranslationUnavailableReason(1, 1, 1, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kMIMETypeUnsupported);
  recorder.CheckMenuTranslationUnavailableReason(1, 1, 1, 1, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kURLNotTranslatable);
  recorder.CheckMenuTranslationUnavailableReason(1, 1, 1, 1, 1, 0, 0, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kTargetLangUnknown);
  recorder.CheckMenuTranslationUnavailableReason(1, 1, 1, 1, 1, 1, 0, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kNotAllowedByPolicy);
  recorder.CheckMenuTranslationUnavailableReason(1, 1, 1, 1, 1, 1, 1, 0);
  TranslateBrowserMetrics::ReportMenuTranslationUnavailableReason(
      TranslateBrowserMetrics::MenuTranslationUnavailableReason::
          kSourceLangUnknown);
  recorder.CheckMenuTranslationUnavailableReason(1, 1, 1, 1, 1, 1, 1, 1);
}

TEST(TranslateBrowserMetricsTest, ReportTranslateHrefHintStatus) {
  MetricsRecorder recorder("Translate.HrefHint.Status");
  recorder.CheckTranslateHrefHintStatus(0, 0, 0, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
      TranslateBrowserMetrics::HrefTranslateStatus::kAutoTranslated);
  recorder.CheckTranslateHrefHintStatus(1, 0, 0, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
      TranslateBrowserMetrics::HrefTranslateStatus::
          kAutoTranslatedDifferentTargetLanguage);
  recorder.CheckTranslateHrefHintStatus(1, 1, 0, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
      TranslateBrowserMetrics::HrefTranslateStatus::kUiShownNotAutoTranslated);
  recorder.CheckTranslateHrefHintStatus(1, 1, 1, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintStatus(
      TranslateBrowserMetrics::HrefTranslateStatus::
          kNoUiShownNotAutoTranslated);
  recorder.CheckTranslateHrefHintStatus(1, 1, 1, 1);
}

}  // namespace
}  // namespace translate
