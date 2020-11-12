// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include "base/metrics/histogram_functions.h"
#include "components/translate/core/browser/translate_manager.h"

namespace translate {

const char kTranslatePageLoadAutofillAssistantDeferredTriggerDecision[] =
    "Translate.PageLoad.AutofillAssistantDeferredTriggerDecision";
const char kTranslatePageLoadFinalState[] = "Translate.PageLoad.FinalState";
const char kTranslatePageLoadInitialState[] = "Translate.PageLoad.InitialState";
const char kTranslatePageLoadNumTranslations[] =
    "Translate.PageLoad.NumTranslations";
const char kTranslatePageLoadNumReversions[] =
    "Translate.PageLoad.NumReversions";
const char kTranslatePageLoadRankerDecision[] =
    "Translate.PageLoad.Ranker.Decision";
const char kTranslatePageLoadRankerVersion[] =
    "Translate.PageLoad.Ranker.Version";
const char kTranslatePageLoadTriggerDecision[] =
    "Translate.PageLoad.TriggerDecision";

TranslateMetricsLoggerImpl::TranslateMetricsLoggerImpl(
    base::WeakPtr<TranslateManager> translate_manager)
    : translate_manager_(translate_manager) {}

TranslateMetricsLoggerImpl::~TranslateMetricsLoggerImpl() = default;

void TranslateMetricsLoggerImpl::OnPageLoadStart(bool is_foreground) {
  if (translate_manager_)
    translate_manager_->RegisterTranslateMetricsLogger(
        weak_method_factory_.GetWeakPtr());

  is_foreground_ = is_foreground;
}

void TranslateMetricsLoggerImpl::OnForegroundChange(bool is_foreground) {
  is_foreground_ = is_foreground;
}

void TranslateMetricsLoggerImpl::RecordMetrics(bool is_final) {
  // The first time |RecordMetrics| is called, record all page load frequency
  // UMA metrcis.
  if (sequence_no_ == 0)
    RecordPageLoadUmaMetrics();

  // TODO(curranmax): Log UKM metrics now that the page load is.
  // completed. https://crbug.com/1114868.

  sequence_no_++;
}

void TranslateMetricsLoggerImpl::RecordPageLoadUmaMetrics() {
  base::UmaHistogramEnumeration(kTranslatePageLoadRankerDecision,
                                ranker_decision_);
  base::UmaHistogramSparse(kTranslatePageLoadRankerVersion,
                           int(ranker_version_));
  base::UmaHistogramEnumeration(kTranslatePageLoadTriggerDecision,
                                trigger_decision_);
  base::UmaHistogramBoolean(
      kTranslatePageLoadAutofillAssistantDeferredTriggerDecision,
      autofill_assistant_deferred_trigger_decision_);

  // If a translation is still in progress, then use the previous state.
  bool this_initial_state_is_translated =
      is_initial_state_dependent_on_in_progress_translation_
          ? previous_state_is_translated_
          : initial_state_is_translated_;
  bool this_current_state_is_translated = is_translation_in_progress_
                                              ? previous_state_is_translated_
                                              : current_state_is_translated_;

  base::UmaHistogramEnumeration(
      kTranslatePageLoadInitialState,
      ConvertToTranslateState(this_initial_state_is_translated,
                              initial_state_is_ui_shown_,
                              initial_state_is_omnibox_icon_shown_));
  base::UmaHistogramEnumeration(
      kTranslatePageLoadFinalState,
      ConvertToTranslateState(this_current_state_is_translated,
                              current_state_is_ui_shown_,
                              current_state_is_omnibox_icon_shown_));
  base::UmaHistogramCounts10000(kTranslatePageLoadNumTranslations,
                                num_translations_);
  base::UmaHistogramCounts10000(kTranslatePageLoadNumReversions,
                                num_reversions_);
}

void TranslateMetricsLoggerImpl::LogRankerMetrics(
    RankerDecision ranker_decision,
    uint32_t ranker_version) {
  ranker_decision_ = ranker_decision;
  ranker_version_ = ranker_version;
}

void TranslateMetricsLoggerImpl::LogTriggerDecision(
    TriggerDecision trigger_decision) {
  // Only stores the first non-kUninitialized trigger decision in the event that
  // there are multiple.
  if (trigger_decision_ == TriggerDecision::kUninitialized)
    trigger_decision_ = trigger_decision;
}

void TranslateMetricsLoggerImpl::LogAutofillAssistantDeferredTriggerDecision() {
  autofill_assistant_deferred_trigger_decision_ = true;
}

void TranslateMetricsLoggerImpl::LogInitialState() {
  // Sets the initial state to the current state.
  initial_state_is_translated_ = current_state_is_translated_;
  initial_state_is_ui_shown_ = current_state_is_ui_shown_;
  initial_state_is_omnibox_icon_shown_ = current_state_is_omnibox_icon_shown_;

  is_initial_state_set_ = true;

  // If the initial state is based on an in progress translation, we may need to
  // update the initial state if the translation fails or if we try to record
  // metrics before it finishes.
  if (is_translation_in_progress_)
    is_initial_state_dependent_on_in_progress_translation_ = true;
}

void TranslateMetricsLoggerImpl::LogTranslationStarted() {
  // Save the previous state in case the translation fails.
  previous_state_is_translated_ = current_state_is_translated_;

  current_state_is_translated_ = true;
  is_translation_in_progress_ = true;
}

void TranslateMetricsLoggerImpl::LogTranslationFinished(bool was_sucessful) {
  if (was_sucessful)
    num_translations_++;
  else {
    // If the translation fails, then undo the change to the current state.
    current_state_is_translated_ = previous_state_is_translated_;

    // Update the initial state if it was dependent on this translation..
    if (is_initial_state_dependent_on_in_progress_translation_)
      initial_state_is_translated_ = previous_state_is_translated_;
  }

  is_translation_in_progress_ = false;
  is_initial_state_dependent_on_in_progress_translation_ = false;
}

void TranslateMetricsLoggerImpl::LogReversion() {
  current_state_is_translated_ = false;
  num_reversions_++;
}

void TranslateMetricsLoggerImpl::LogUIChange(bool is_ui_shown) {
  current_state_is_ui_shown_ = is_ui_shown;
}

void TranslateMetricsLoggerImpl::LogOmniboxIconChange(
    bool is_omnibox_icon_shown) {
  current_state_is_omnibox_icon_shown_ = is_omnibox_icon_shown;
}

TranslateState TranslateMetricsLoggerImpl::ConvertToTranslateState(
    bool is_translated,
    bool is_ui_shown,
    bool is_omnibox_shown) const {
  if (!is_initial_state_set_)
    return TranslateState::kUninitialized;

  if (!is_translated && !is_ui_shown && !is_omnibox_shown)
    return TranslateState::kNotTranslatedNoUI;

  if (!is_translated && !is_ui_shown && is_omnibox_shown)
    return TranslateState::kNotTranslatedOmniboxIconOnly;

  if (!is_translated && is_ui_shown)
    return TranslateState::kNotTranslatedUIShown;

  if (is_translated && !is_ui_shown && !is_omnibox_shown)
    return TranslateState::kTranslatedNoUI;

  if (is_translated && !is_ui_shown && is_omnibox_shown)
    return TranslateState::kTranslatedOmniboxIconOnly;

  if (is_translated && is_ui_shown)
    return TranslateState::kTranslatedUIShown;

  NOTREACHED();
  return TranslateState::kUninitialized;
}

}  // namespace translate
