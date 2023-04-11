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

  void CheckInitiationStatus(
      int expected_disabled_by_prefs,
      int expected_disabled_by_config,
      int expected_disabled_by_build,
      int expected_language_is_not_supported,
      int expected_mime_type_is_not_supported,
      int expected_url_is_not_supported,
      int expected_similar_languages,
      int expected_accept_languages,
      int expected_auto_by_config,
      int expected_auto_by_link,
      int expected_show_infobar,
      int expected_language_in_ulp,
      int expected_aborted_by_ranker,
      int expected_aborted_by_matches_previous_language) {
    Snapshot();

    EXPECT_EQ(
        expected_disabled_by_prefs,
        GetCountWithoutSnapshot(
            TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_PREFS));
    EXPECT_EQ(
        expected_disabled_by_config,
        GetCountWithoutSnapshot(
            TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG));
    EXPECT_EQ(expected_disabled_by_build,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_KEY));
    EXPECT_EQ(expected_language_is_not_supported,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::
                      INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED));
    EXPECT_EQ(expected_mime_type_is_not_supported,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::
                      INITIATION_STATUS_MIME_TYPE_IS_NOT_SUPPORTED));
    EXPECT_EQ(
        expected_url_is_not_supported,
        GetCountWithoutSnapshot(
            TranslateBrowserMetrics::INITIATION_STATUS_URL_IS_NOT_SUPPORTED));
    EXPECT_EQ(
        expected_similar_languages,
        GetCountWithoutSnapshot(
            TranslateBrowserMetrics::INITIATION_STATUS_SIMILAR_LANGUAGES));
    EXPECT_EQ(expected_accept_languages,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::INITIATION_STATUS_ACCEPT_LANGUAGES));
    EXPECT_EQ(expected_auto_by_config,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_CONFIG));
    EXPECT_EQ(expected_auto_by_link,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_LINK));
    EXPECT_EQ(expected_show_infobar,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::INITIATION_STATUS_SHOW_INFOBAR));
    EXPECT_EQ(expected_language_in_ulp,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::INITIATION_STATUS_LANGUAGE_IN_ULP));
    EXPECT_EQ(
        expected_aborted_by_ranker,
        GetCountWithoutSnapshot(
            TranslateBrowserMetrics::INITIATION_STATUS_ABORTED_BY_RANKER));
    EXPECT_EQ(expected_aborted_by_matches_previous_language,
              GetCountWithoutSnapshot(
                  TranslateBrowserMetrics::
                      INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE));
  }

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

  void CheckTranslateHrefHintPrefsFilterStatus(
      int expected_not_in_blocklists,
      int expected_language_in_blocklist,
      int expected_site_in_blocklist,
      int expected_both_language_and_site_in_blocklist) {
    Snapshot();

    EXPECT_EQ(expected_not_in_blocklists,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
                      kNotInBlocklists)));
    EXPECT_EQ(expected_language_in_blocklist,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
                      kLanguageInBlocklist)));
    EXPECT_EQ(expected_site_in_blocklist,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
                      kSiteInBlocklist)));
    EXPECT_EQ(expected_both_language_and_site_in_blocklist,
              GetCountWithoutSnapshot(static_cast<int>(
                  TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
                      kBothLanguageAndSiteInBlocklist)));
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

TEST(TranslateBrowserMetricsTest, ReportInitiationStatus) {
  MetricsRecorder recorder("Translate.InitiationStatus.v2");

  recorder.CheckInitiationStatus(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_PREFS);
  recorder.CheckInitiationStatus(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_CONFIG);
  recorder.CheckInitiationStatus(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_DISABLED_BY_KEY);
  recorder.CheckInitiationStatus(1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_MIME_TYPE_IS_NOT_SUPPORTED);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_URL_IS_NOT_SUPPORTED);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_SIMILAR_LANGUAGES);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_ACCEPT_LANGUAGES);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_CONFIG);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_AUTO_BY_LINK);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_SHOW_INFOBAR);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_LANGUAGE_IN_ULP);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::INITIATION_STATUS_ABORTED_BY_RANKER);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0);
  TranslateBrowserMetrics::ReportInitiationStatus(
      TranslateBrowserMetrics::
          INITIATION_STATUS_ABORTED_BY_MATCHES_PREVIOUS_LANGUAGE);
  recorder.CheckInitiationStatus(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
}

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

TEST(TranslateBrowserMetricsTest, ReportedUnsupportedLanguageAtInitiation) {
  const int ENGLISH = 25966;

  MetricsRecorder recorder("Translate.UnsupportedLanguageAtInitiation");
  EXPECT_EQ(0, recorder.GetTotalCount());
  TranslateBrowserMetrics::ReportUnsupportedLanguageAtInitiation("en");
  EXPECT_EQ(1, recorder.GetCount(ENGLISH));
}

TEST(TranslateBrowserMetricsTest, ReportedTranslateSourceLanguage) {
  const int ENGLISH = -74147910;
  const int FRENCH = 1704315002;

  MetricsRecorder recorder("Translate.SourceLanguage");
  EXPECT_EQ(0, recorder.GetTotalCount());

  TranslateBrowserMetrics::ReportTranslateSourceLanguage("en");
  TranslateBrowserMetrics::ReportTranslateSourceLanguage("fr");
  TranslateBrowserMetrics::ReportTranslateSourceLanguage("en");

  EXPECT_EQ(2, recorder.GetCount(ENGLISH));
  EXPECT_EQ(1, recorder.GetCount(FRENCH));
}

TEST(TranslateBrowserMetricsTest, ReportedTranslateTargetLanguage) {
  const int ENGLISH = -74147910;
  const int FRENCH = 1704315002;

  MetricsRecorder recorder("Translate.TargetLanguage");
  EXPECT_EQ(0, recorder.GetTotalCount());

  TranslateBrowserMetrics::ReportTranslateTargetLanguage("en");
  TranslateBrowserMetrics::ReportTranslateTargetLanguage("fr");
  TranslateBrowserMetrics::ReportTranslateTargetLanguage("en");

  EXPECT_EQ(2, recorder.GetCount(ENGLISH));
  EXPECT_EQ(1, recorder.GetCount(FRENCH));
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

TEST(TranslateBrowserMetricsTest, ReportTranslateHrefHintPrefsFilterStatus) {
  MetricsRecorder recorder("Translate.HrefHint.PrefsFilterStatus");
  recorder.CheckTranslateHrefHintPrefsFilterStatus(0, 0, 0, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintPrefsFilterStatus(
      TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
          kNotInBlocklists);
  recorder.CheckTranslateHrefHintPrefsFilterStatus(1, 0, 0, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintPrefsFilterStatus(
      TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
          kLanguageInBlocklist);
  recorder.CheckTranslateHrefHintPrefsFilterStatus(1, 1, 0, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintPrefsFilterStatus(
      TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
          kSiteInBlocklist);
  recorder.CheckTranslateHrefHintPrefsFilterStatus(1, 1, 1, 0);
  TranslateBrowserMetrics::ReportTranslateHrefHintPrefsFilterStatus(
      TranslateBrowserMetrics::HrefTranslatePrefsFilterStatus::
          kBothLanguageAndSiteInBlocklist);
  recorder.CheckTranslateHrefHintPrefsFilterStatus(1, 1, 1, 1);
}

}  // namespace
}  // namespace translate
