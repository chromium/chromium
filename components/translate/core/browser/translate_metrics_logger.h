// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_

#include <stdint.h>

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
  kMaxValue = kAutomaticTranslationByPref,
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

  virtual void LogRankerMetrics(RankerDecision ranker_decision,
                                uint32_t ranker_version) = 0;

  // Records trigger decision that impacts the initial state of Translate. The
  // highest priority trigger decision will be logged to UMA at the end of the
  // page load.
  virtual void LogTriggerDecision(TriggerDecision trigger_decision) = 0;
  virtual void LogAutofillAssistantDeferredTriggerDecision() = 0;

  // Tracks the state of Translate over the course of the page load.
  virtual void LogInitialState() = 0;
  virtual void LogTranslationStarted() = 0;
  virtual void LogTranslationFinished(bool was_successful) = 0;
  virtual void LogReversion() = 0;
  virtual void LogUIChange(bool is_ui_shown) = 0;
  virtual void LogOmniboxIconChange(bool is_omnibox_icon_show) = 0;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_
