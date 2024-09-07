// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_

#include <stdint.h>
#include <string>

#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/common/translate_errors.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace translate {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RankerDecision {
  kUninitialized = 0,
  kNotQueried = 1,
  kShowUI = 2,
  kDontShowUI = 3,
  kMaxValue = kDontShowUI,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TranslateState {
  kUninitialized = 0,
  kNotTranslatedNoUI = 1,
  kNotTranslatedOmniboxIconOnly = 2,
  kNotTranslatedUIShown = 3,
  kTranslatedNoUI = 4,
  kTranslatedOmniboxIconOnly = 5,
  kTranslatedUIShown = 6,
  kMaxValue = kTranslatedUIShown,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TranslationStatus {
  kUninitialized = 0,
  // kSuccessFromManualTranslation = 1,  // no longer used, split into
  // kSuccessFromManualUiTranslation and
  // kSuccessFromManualContextMenuTranslation enum values.
  kSuccessFromAutomaticTranslationByPref = 2,
  kSuccessFromAutomaticTranslationByLink = 3,
  // kManualTranslation = 4,  // no longer used, split into
  // kRevertedManualUiTranslation and
  // kRevertedManualContextMenuTranslation enum values.
  kRevertedAutomaticTranslation = 5,
  kNewTranslation = 6,
  kTranslationAbandoned = 7,
  // kFailedWithNoErrorManualTranslation = 8,  // no longer used, split into
  // kFailedWithNoErrorManualUiTranslation and
  // kFailedWithNoErrorManualContextMenuTranslation enum values.
  kFailedWithNoErrorAutomaticTranslation = 9,
  // kFailedWithErrorManualTranslation = 10,  // no longer used, split into
  // kFailedWithErrorManualUiTranslation and
  // kFailedWithErrorManualContextMenuTranslation enum values.
  kFailedWithErrorAutomaticTranslation = 11,
  kSuccessFromManualUiTranslation = 12,
  kRevertedManualUiTranslation = 13,
  kFailedWithNoErrorManualUiTranslation = 14,
  kFailedWithErrorManualUiTranslation = 15,
  kSuccessFromManualContextMenuTranslation = 16,
  kRevertedManualContextMenuTranslation = 17,
  kFailedWithNoErrorManualContextMenuTranslation = 18,
  kFailedWithErrorManualContextMenuTranslation = 19,
  kSuccessFromAutomaticTranslationToPredefinedTarget = 20,
  kRevertedAutomaticTranslationToPredefinedTarget = 21,
  kFailedWithNoErrorAutomaticTranslationToPredefinedTarget = 22,
  kFailedWithErrorAutomaticTranslationToPredefinedTarget = 23,
  kSuccessFromAutomaticTranslationByHref = 24,
  kRevertedAutomaticTranslationByHref = 25,
  kFailedWithNoErrorAutomaticTranslationByHref = 26,
  kFailedWithErrorAutomaticTranslationByHref = 27,
  kMaxValue = kFailedWithErrorAutomaticTranslationByHref,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TranslationType {
  kUninitialized = 0,
  // kManualInitialTranslation = 1,  // no longer used, split into
  // kManualUiInitialTranslation and kManualContextMenuInitialTranslation
  // kManualReTranslation = 2,  // no longer used, split into
  // kManualUiReTranslation and kManualContextMenuReTranslation
  kAutomaticTranslationByPref = 3,
  kAutomaticTranslationByLink = 4,
  kManualUiInitialTranslation = 5,
  kManualUiReTranslation = 6,
  kManualContextMenuInitialTranslation = 7,
  kManualContextMenuReTranslation = 8,
  kAutomaticTranslationToPredefinedTarget = 9,
  kAutomaticTranslationByHref = 10,
  kMaxValue = kAutomaticTranslationByHref,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TriggerDecision {
  kUninitialized = 0,
  kDisabledDoesntNeedTranslation = 1,
  kDisabledTranslationFeatureDisabled = 2,
  kDisabledOffline = 3,
  kDisabledMissingAPIKey = 4,
  kDisabledMIMETypeNotSupported = 5,
  kDisabledURLNotSupported = 6,
  kDisabledNeverOfferTranslations = 7,
  kDisabledSimilarLanguages = 8,
  kDisabledUnsupportedLanguage = 9,
  kDisabledNeverTranslateLanguage = 10,
  kDisabledNeverTranslateSite = 11,
  kDisabledByRanker = 12,
  kShowUI = 13,
  kAutomaticTranslationByLink = 14,
  kAutomaticTranslationByPref = 15,
  kShowUIFromHref = 16,
  kAutomaticTranslationByHref = 17,
  kAutomaticTranslationToPredefinedTarget = 18,
  kShowIcon = 19,
  kDisabledMatchesPreviousLanguage = 20,
  kMaxValue = kDisabledMatchesPreviousLanguage,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// |TranslateUIInteraction| in translate/enums.xml.
enum class UIInteraction {
  kUninitialized = 0,
  kTranslate = 1,
  kRevert = 2,
  // kAlwaysTranslateLanguage = 3, // no longer used, split into
  // kAddAlwaysTranslateLanguage and kRemoveAlwaysTranslateLanguage
  kChangeSourceLanguage = 4,
  kChangeTargetLanguage = 5,
  // kNeverTranslateLanguage = 6, // no longer used, split into
  // kAddNeverTranslateLanguage and kRemoveNeverTranslateLanguage
  // kNeverTranslateSite = 7, // no longer used, split into
  // kAddNeverTranslateSite and kRemoveNeverTranslateSite
  kCloseUIExplicitly = 8,
  kCloseUILostFocus = 9,
  kCloseUITimerRanOut = 10,
  kAddAlwaysTranslateLanguage = 11,
  kRemoveAlwaysTranslateLanguage = 12,
  kAddNeverTranslateLanguage = 13,
  kRemoveNeverTranslateLanguage = 14,
  kAddNeverTranslateSite = 15,
  kRemoveNeverTranslateSite = 16,
  kOpenLanguageSettings = 17,
  kMaxValue = kOpenLanguageSettings,
};

// TranslateMetricsLogger tracks and logs various UKM and UMA metrics for Chrome
// Translate over the course of a page load.
class TranslateMetricsLogger {
 public:
  TranslateMetricsLogger() = default;
  virtual ~TranslateMetricsLogger() = default;

  TranslateMetricsLogger(const TranslateMetricsLogger&) = delete;
  TranslateMetricsLogger& operator=(const TranslateMetricsLogger&) = delete;

  // Tracks the state of the page over the course of a page load.
  virtual void OnPageLoadStart(bool is_foreground) = 0;
  virtual void OnForegroundChange(bool is_foreground) = 0;

  // Logs all stored page load metrics. If is_final is |true| then RecordMetrics
  // won't be called again.
  virtual void RecordMetrics(bool is_final) = 0;

  // Sets the UKM source ID for the current page load.
  virtual void SetUkmSourceId(ukm::SourceId ukm_source_id) = 0;

  // Tracks information about the Translate Ranker.
  virtual void LogRankerMetrics(RankerDecision ranker_decision,
                                uint32_t ranker_version) = 0;
  virtual void LogRankerStart() = 0;
  virtual void LogRankerFinish() = 0;

  // Records trigger decision that impacts the initial state of Translate. The
  // highest priority trigger decision will be logged to UMA at the end of the
  // page load.
  virtual void LogTriggerDecision(TriggerDecision trigger_decision) = 0;

  // Tracks the state of Translate over the course of the page load.
  virtual void LogInitialState() = 0;
  virtual void LogTranslationStarted(TranslationType translation_type) = 0;
  virtual void LogTranslationFinished(bool was_successful,
                                      TranslateErrors error_type) = 0;
  virtual void LogReversion() = 0;
  virtual void LogUIChange(bool is_ui_shown) = 0;
  virtual void LogOmniboxIconChange(bool is_omnibox_icon_show) = 0;

  // Used to record the source language and target language both initially and
  // if the user changes these values.
  virtual void LogInitialSourceLanguage(const std::string& source_language_code,
                                        bool is_in_users_content_language) = 0;
  virtual void LogSourceLanguage(const std::string& source_language_code) = 0;
  virtual void LogTargetLanguage(
      const std::string& target_language_code,
      TranslateBrowserMetrics::TargetLanguageOrigin target_language_origin) = 0;

  // Used to record the language attributes specified by the HTML document.
  // Recorded for each language detection.
  virtual void LogHTMLDocumentLanguage(
      const std::string& html_doc_language) = 0;
  virtual void LogHTMLContentLanguage(
      const std::string& html_content_language) = 0;

  // Used to record the language detection model's prediction and reliability
  // based on the page content's text. Recorded for each language detection.
  virtual void LogDetectedLanguage(const std::string& detected_language) = 0;
  virtual void LogDetectionReliabilityScore(
      const float& model_detection_reliability_score) = 0;

  // Records the user's high level interactions with the Translate UI.
  virtual void LogUIInteraction(UIInteraction ui_interaction) = 0;

  // Returns the translation type of the next manual translation.
  virtual TranslationType GetNextManualTranslationType(
      bool is_context_menu_initiated_translation) = 0;

  virtual void SetHasHrefTranslateTarget(bool has_href_translate_target) = 0;

  // Records whether the page content used to detect the page language
  // was empty or not.
  virtual void LogWasContentEmpty(bool was_content_empty) = 0;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_
