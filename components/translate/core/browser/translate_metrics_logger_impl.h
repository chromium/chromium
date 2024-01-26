// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_prefs.h"

namespace base {
class TickClock;
}  // namespace base

namespace translate {

// Translation frequency UMA histograms.
extern const char kTranslateTranslationSourceLanguage[];
extern const char kTranslateTranslationTargetLanguage[];
extern const char kTranslateTranslationTargetLanguageOrigin[];
extern const char kTranslateTranslationStatus[];
extern const char kTranslateTranslationType[];

// UI Interaction frequency UMA histograms.
extern const char kTranslateUiInteractionEvent[];

// Page-load frequency UMA histograms.
extern const char kTranslatePageLoadFinalSourceLanguage[];
extern const char kTranslatePageLoadFinalState[];
extern const char kTranslatePageLoadFinalTargetLanguage[];
extern const char kTranslatePageLoadHrefTriggerDecision[];
extern const char kTranslatePageLoadInitialSourceLanguage[];
extern const char kTranslatePageLoadInitialState[];
extern const char kTranslatePageLoadInitialTargetLanguage[];
extern const char kTranslatePageLoadInitialTargetLanguageOrigin[];
extern const char
    kTranslatePageLoadIsInitialSourceLanguageInUsersContentLanguages[];
extern const char kTranslatePageLoadNumTargetLanguageChanges[];
extern const char kTranslatePageLoadNumTranslations[];
extern const char kTranslatePageLoadNumReversions[];
extern const char kTranslatePageLoadRankerDecision[];
extern const char kTranslatePageLoadRankerTimerShouldOfferTranslation[];
extern const char kTranslatePageLoadRankerVersion[];
extern const char kTranslatePageLoadTriggerDecision[];

// Session frequency UMA histograms.
extern const char kTranslateApplicationStartAlwaysTranslateLanguage[];
extern const char kTranslateApplicationStartAlwaysTranslateLanguageCount[];
extern const char kTranslateApplicationStartNeverTranslateLanguage[];
extern const char kTranslateApplicationStartNeverTranslateLanguageCount[];
extern const char kTranslateApplicationStartNeverTranslateSiteCount[];

class NullTranslateMetricsLogger : public TranslateMetricsLogger {
 public:
  NullTranslateMetricsLogger() = default;

  // TranslateMetricsLogger
  void OnPageLoadStart(bool is_foreground) override {}
  void OnForegroundChange(bool is_foreground) override {}
  void RecordMetrics(bool is_final) override {}
  void SetUkmSourceId(ukm::SourceId ukm_source_id) override {}
  void LogRankerMetrics(RankerDecision ranker_decision,
                        uint32_t ranker_version) override {}
  void LogRankerStart() override {}
  void LogRankerFinish() override {}
  void LogTriggerDecision(TriggerDecision trigger_decision) override {}
  void LogInitialState() override {}
  void LogTranslationStarted(TranslationType translation_type) override {}
  void LogTranslationFinished(bool was_successful,
                              TranslateErrors error_type) override {}
  void LogReversion() override {}
  void LogUIChange(bool is_ui_shown) override {}
  void LogOmniboxIconChange(bool is_omnibox_icon_shown) override {}
  void LogInitialSourceLanguage(const std::string& source_language_code,
                                bool is_in_users_content_languages) override {}
  void LogSourceLanguage(const std::string& source_language_code) override {}
  void LogTargetLanguage(const std::string& target_language_code,
                         TranslateBrowserMetrics::TargetLanguageOrigin
                             target_language_origin) override {}
  void LogHTMLDocumentLanguage(const std::string& html_doc_language) override {}
  void LogHTMLContentLanguage(
      const std::string& html_content_language) override {}
  void LogDetectedLanguage(const std::string& detected_language) override {}
  void LogDetectionReliabilityScore(
      const float& model_detection_reliability_score) override {}
  void LogUIInteraction(UIInteraction ui_interaction) override {}
  TranslationType GetNextManualTranslationType(
      bool is_context_menu_initiated_translation) override;
  void SetHasHrefTranslateTarget(bool has_href_translate_target) override {}
  void LogWasContentEmpty(bool was_content_empty) override {}
};

class TranslateManager;

namespace testing {
class TranslateMetricsLoggerImplTest;
}  // namespace testing

// TranslateMetricsLogger tracks and logs various UKM and UMA metrics for Chrome
// Translate over the course of a page load.
class TranslateMetricsLoggerImpl : public TranslateMetricsLogger {
 public:
  explicit TranslateMetricsLoggerImpl(
      base::WeakPtr<TranslateManager> translate_manager);
  ~TranslateMetricsLoggerImpl() override;

  TranslateMetricsLoggerImpl(const TranslateMetricsLoggerImpl&) = delete;
  TranslateMetricsLoggerImpl& operator=(const TranslateMetricsLoggerImpl&) =
      delete;

  static void LogApplicationStartMetrics(
      std::unique_ptr<TranslatePrefs> translate_prefs);

  // Overrides the clock used to track the time of certain actions. Should only
  // be used for testing purposes.
  void SetInternalClockForTesting(base::TickClock* clock);

  // TranslateMetricsLogger
  void OnPageLoadStart(bool is_foreground) override;
  void OnForegroundChange(bool is_foreground) override;
  void RecordMetrics(bool is_final) override;
  void SetUkmSourceId(ukm::SourceId ukm_source_id) override;
  void LogRankerMetrics(RankerDecision ranker_decision,
                        uint32_t ranker_version) override;
  void LogRankerStart() override;
  void LogRankerFinish() override;
  void LogTriggerDecision(TriggerDecision trigger_decision) override;
  void LogInitialState() override;
  void LogTranslationStarted(TranslationType translation_type) override;
  void LogTranslationFinished(bool was_successful,
                              TranslateErrors error_type) override;
  void LogReversion() override;
  void LogUIChange(bool is_ui_shown) override;
  void LogOmniboxIconChange(bool is_omnibox_icon_shown) override;
  void LogInitialSourceLanguage(const std::string& source_language_code,
                                bool is_in_users_content_languages) override;
  void LogSourceLanguage(const std::string& source_language_code) override;
  void LogTargetLanguage(const std::string& target_language_code,
                         TranslateBrowserMetrics::TargetLanguageOrigin
                             target_language_origin) override;
  void LogHTMLDocumentLanguage(const std::string& html_doc_language) override;
  void LogHTMLContentLanguage(
      const std::string& html_content_language) override;
  void LogDetectedLanguage(const std::string& detected_language) override;
  void LogDetectionReliabilityScore(
      const float& model_detection_reliability_score) override;
  void LogUIInteraction(UIInteraction ui_interaction) override;
  TranslationType GetNextManualTranslationType(
      bool is_context_menu_initiated_translation) override;
  void SetHasHrefTranslateTarget(bool has_href_translate_target) override;
  void LogWasContentEmpty(bool was_content_empty) override;

  // TODO(curranmax): Add appropriate functions for the Translate code to log
  // relevant events. https://crbug.com/1114868.
 private:
  friend class testing::TranslateMetricsLoggerImplTest;

  // Logs all page load frequency UMA metrics based on the stored state.
  void RecordPageLoadUmaMetrics(bool initial_state_is_translated,
                                bool current_stat_is_translated);

  // Logs all relevant information about a translation.
  void RecordTranslationHistograms(
      TranslationType translation_type,
      const std::string& source_language,
      const std::string& target_language,
      TranslateBrowserMetrics::TargetLanguageOrigin target_language_origin);

  // Logs the final status of the translation.
  void RecordTranslationStatus(TranslationStatus translation_status);

  // Helper function to convert the given |TranslationType| to the appropriate
  // |TranslationStatus| on a successful translation.
  TranslationStatus ConvertTranslationTypeToRevertedTranslationStatus(
      TranslationType translation_type);
  TranslationStatus ConvertTranslationTypeToFailedTranslationStatus(
      TranslationType translation_type,
      bool was_translate_error);
  TranslationStatus ConvertTranslationTypeToSuccessfulTranslationStatus(
      bool is_translation_in_progress,
      TranslationType translation_type);

  // Helper function to get the correct |TranslateState| value based on the
  // different dimensions we care about.
  TranslateState ConvertToTranslateState(bool is_translated,
                                         bool is_ui_shown,
                                         bool is_omnibox_shown) const;

  // Updates |total_time_translated_| and |total_time_not_translated_|. This
  // function is only called immediately before the translated or foreground
  // state is changed, and the input must be the state before the change.
  void UpdateTimeTranslated(bool was_translated, bool was_foreground);

  base::WeakPtr<TranslateManager> translate_manager_;

  // Since |RecordMetrics()| can be called multiple times, such as when Chrome
  // is backgrounded and reopened, we use |sequence_no_| to differentiate the
  // recorded UKM protos.
  unsigned int sequence_no_ = 0;

  // The UKM source ID for the current page load.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // Tracks if the associated page is in the foreground (|true|) or the
  // background (|false|)
  bool is_foreground_ = false;

  // Stores state about TranslateRanker for this page load.
  RankerDecision ranker_decision_ = RankerDecision::kUninitialized;
  uint32_t ranker_version_ = 0;
  base::TimeTicks ranker_start_time_;
  std::optional<base::TimeDelta> ranker_duration_;

  // Stores the reason for the initial state of the page load. In the case there
  // are multiple reasons, only the first reported reason is stored.
  TriggerDecision trigger_decision_ = TriggerDecision::kUninitialized;

  // Tracks the different dimensions that determine the state of Translate.
  bool is_initial_state_set_ = false;

  bool initial_state_is_translated_ = false;
  bool initial_state_is_ui_shown_ = false;
  bool initial_state_is_omnibox_icon_shown_ = false;

  bool current_state_is_translated_ = false;
  bool current_state_is_ui_shown_ = false;
  bool current_state_is_omnibox_icon_shown_ = false;

  bool previous_state_is_translated_ = false;

  bool is_translation_in_progress_ = false;
  bool is_initial_state_dependent_on_in_progress_translation_ = false;

  // Tracks the number of times the page is translated and the translastion is
  // reverted.
  int num_translations_ = 0;
  int num_reversions_ = 0;

  // Used to track the time it takes to translate. Specifically this will be the
  // time between when LogTranslationStarted is called and
  // LogTranslationFinished is called. It will therefore include all aspects of
  // translation include: loading the translate script, loading dependent
  // libraries, and running the translate script.
  base::TimeTicks time_of_last_translation_start_;
  base::TimeDelta max_time_to_translate_;

  // Tracks the amount of time the page is in the foreground and either
  // translated or not translated.
  raw_ptr<const base::TickClock> clock_;
  base::TimeTicks time_of_last_state_change_;
  base::TimeDelta total_time_translated_;
  base::TimeDelta total_time_not_translated_;

  // Tracks the source and target language over the course of the page load.
  std::string initial_source_language_;
  std::string current_source_language_;
  bool is_initial_source_language_in_users_content_languages_ = false;

  std::string initial_target_language_;
  std::string current_target_language_;
  int num_target_language_changes_ = 0;
  TranslateBrowserMetrics::TargetLanguageOrigin
      initial_target_language_origin_ =
          TranslateBrowserMetrics::TargetLanguageOrigin::kUninitialized;
  TranslateBrowserMetrics::TargetLanguageOrigin
      current_target_language_origin_ =
          TranslateBrowserMetrics::TargetLanguageOrigin::kUninitialized;

  // Tracks this record's HTML language attributes.
  std::string html_doc_language_;
  std::string html_content_language_;

  // Tracks this record's language model's prediction and reliability.
  std::string model_detected_language_;
  float model_detection_reliability_score_ = 0.0;

  // Tracks any translation errors that occur over the course of the page load.
  TranslateErrors first_translate_error_type_ = TranslateErrors::NONE;
  int num_translate_errors_ = 0;

  // Tracks the user's high level interaction with the Translate UI over the
  // course of a page load.
  UIInteraction first_ui_interaction_ = UIInteraction::kUninitialized;
  int num_ui_interactions_ = 0;

  // Tracks the status of the most recent translation.
  bool is_translation_status_pending_ = false;
  TranslationType current_translation_type_ = TranslationType::kUninitialized;

  // Tracks if any translations has started on this page load.
  bool has_any_translation_started_ = false;

  // Tracks if this page load has an href translate target language on a link
  // from Google Search.
  bool has_href_translate_target_ = false;

  // Tracks whether the page content used to detect the page language
  // was empty or not.
  bool was_content_empty_ = true;

  base::WeakPtrFactory<TranslateMetricsLoggerImpl> weak_method_factory_{this};
};

}  // namespace translate

// TODO(curranmax): Add unit tests for this class. https://crbug.com/1114868.

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
