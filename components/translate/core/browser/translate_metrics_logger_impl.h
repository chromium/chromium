// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/translate_metrics_logger.h"

namespace translate {

extern const char kTranslatePageLoadAutofillAssistantDeferredTriggerDecision[];
extern const char kTranslatePageLoadFinalState[];
extern const char kTranslatePageLoadInitialState[];
extern const char kTranslatePageLoadNumTranslations[];
extern const char kTranslatePageLoadNumReversions[];
extern const char kTranslatePageLoadRankerDecision[];
extern const char kTranslatePageLoadRankerVersion[];
extern const char kTranslatePageLoadTriggerDecision[];

class NullTranslateMetricsLogger : public TranslateMetricsLogger {
 public:
  NullTranslateMetricsLogger() = default;

  // TranslateMetricsLogger
  void OnPageLoadStart(bool is_foreground) override {}
  void OnForegroundChange(bool is_foreground) override {}
  void RecordMetrics(bool is_final) override {}
  void LogRankerMetrics(RankerDecision ranker_decision,
                        uint32_t ranker_version) override {}
  void LogTriggerDecision(TriggerDecision trigger_decision) override {}
  void LogAutofillAssistantDeferredTriggerDecision() override {}
  void LogInitialState() override {}
  void LogTranslationStarted() override {}
  void LogTranslationFinished(bool was_sucessful) override {}
  void LogReversion() override {}
  void LogUIChange(bool is_ui_shown) override {}
  void LogOmniboxIconChange(bool is_omnibox_icon_shown) override {}
};

class TranslateManager;

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

  // TranslateMetricsLogger
  void OnPageLoadStart(bool is_foreground) override;
  void OnForegroundChange(bool is_foreground) override;
  void RecordMetrics(bool is_final) override;
  void LogRankerMetrics(RankerDecision ranker_decision,
                        uint32_t ranker_version) override;
  void LogTriggerDecision(TriggerDecision trigger_decision) override;
  void LogAutofillAssistantDeferredTriggerDecision() override;
  void LogInitialState() override;
  void LogTranslationStarted() override;
  void LogTranslationFinished(bool was_sucessful) override;
  void LogReversion() override;
  void LogUIChange(bool is_ui_shown) override;
  void LogOmniboxIconChange(bool is_omnibox_icon_shown) override;

  // TODO(curranmax): Add appropriate functions for the Translate code to log
  // relevant events. https://crbug.com/1114868.
 private:
  // Logs all page load frequency UMA metrics based on the stored state.
  void RecordPageLoadUmaMetrics();

  // Helpter function to get the correct |TranslateState| value based on the
  // different dimensions we care about.
  TranslateState ConvertToTranslateState(bool is_translated,
                                         bool is_ui_shown,
                                         bool is_omnibox_shown) const;

  base::WeakPtr<TranslateManager> translate_manager_;

  // Since |RecordMetrics()| can be called multiple times, such as when Chrome
  // is backgrounded and reopened, we use |sequence_no_| to differentiate the
  // recorded UKM protos.
  unsigned int sequence_no_ = 0;

  // Tracks if the associated page is in the foreground (|true|) or the
  // background (|false|)
  bool is_foreground_ = false;

  // Stores state about TranslateRanker for this page load.
  RankerDecision ranker_decision_ = RankerDecision::kUninitialized;
  uint32_t ranker_version_ = 0;

  // Stores the reason for the initial state of the page load. In the case there
  // are multiple reasons, only the first reported reason is stored.
  TriggerDecision trigger_decision_ = TriggerDecision::kUninitialized;
  bool autofill_assistant_deferred_trigger_decision_ = false;

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

  base::WeakPtrFactory<TranslateMetricsLoggerImpl> weak_method_factory_{this};
};

}  // namespace translate

// TODO(curranmax): Add unit tests for this class. https://crbug.com/1114868.

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
