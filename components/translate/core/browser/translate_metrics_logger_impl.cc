// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/time/default_tick_clock.h"
#include "components/translate/core/browser/translate_manager.h"

namespace translate {

const char kTranslatePageLoadAutofillAssistantDeferredTriggerDecision[] =
    "Translate.PageLoad.AutofillAssistantDeferredTriggerDecision";
const char kTranslatePageLoadFinalSourceLanguage[] =
    "Translate.PageLoad.FinalSourceLanguage";
const char kTranslatePageLoadFinalState[] = "Translate.PageLoad.FinalState";
const char kTranslatePageLoadFinalTargetLanguage[] =
    "Translate.PageLoad.FinalTargetLanguage";
const char kTranslatePageLoadInitialSourceLanguage[] =
    "Translate.PageLoad.InitialSourceLanguage";
const char kTranslatePageLoadInitialState[] = "Translate.PageLoad.InitialState";
const char kTranslatePageLoadInitialTargetLanguage[] =
    "Translate.PageLoad.InitialTargetLanguage";
const char kTranslatePageLoadIsInitialSourceLanguageInUsersContentLanguages[] =
    "Translate.PageLoad.IsInitialSourceLanguageInUsersContentLanguages";
const char kTranslatePageLoadNumTargetLanguageChanges[] =
    "Translate.PageLoad.NumTargetLanguageChanges";
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
    : translate_manager_(translate_manager),
      clock_(base::DefaultTickClock::GetInstance()),
      time_of_last_state_change_(clock_->NowTicks()) {}

TranslateMetricsLoggerImpl::~TranslateMetricsLoggerImpl() = default;

void TranslateMetricsLoggerImpl::OnPageLoadStart(bool is_foreground) {
  if (translate_manager_)
    translate_manager_->RegisterTranslateMetricsLogger(
        weak_method_factory_.GetWeakPtr());

  is_foreground_ = is_foreground;
  time_of_last_state_change_ = clock_->NowTicks();
}

void TranslateMetricsLoggerImpl::OnForegroundChange(bool is_foreground) {
  UpdateTimeTranslated(current_state_is_translated_, is_foreground_);
  is_foreground_ = is_foreground;
}

void TranslateMetricsLoggerImpl::RecordMetrics(bool is_final) {
  UpdateTimeTranslated(current_state_is_translated_, is_foreground_);

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

  base::UmaHistogramSparse(kTranslatePageLoadInitialSourceLanguage,
                           base::HashMetricName(initial_source_language_));
  base::UmaHistogramSparse(kTranslatePageLoadFinalSourceLanguage,
                           base::HashMetricName(current_source_language_));
  base::UmaHistogramBoolean(
      kTranslatePageLoadIsInitialSourceLanguageInUsersContentLanguages,
      is_initial_source_language_in_users_content_languages_);
  base::UmaHistogramSparse(kTranslatePageLoadInitialTargetLanguage,
                           base::HashMetricName(initial_target_language_));
  base::UmaHistogramSparse(kTranslatePageLoadFinalTargetLanguage,
                           base::HashMetricName(current_target_language_));
  base::UmaHistogramCustomCounts(kTranslatePageLoadNumTargetLanguageChanges,
                                 num_target_language_changes_, 1, 50, 20);
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

void TranslateMetricsLoggerImpl::LogTranslationFinished(
    TranslateErrors::Type error_type) {
  // The translation succeeded if and only if there were no translation errors.
  if (error_type == TranslateErrors::NONE) {
    UpdateTimeTranslated(previous_state_is_translated_, is_foreground_);
    num_translations_++;
  } else {
    // If the translation fails, then undo the change to the current state.
    current_state_is_translated_ = previous_state_is_translated_;

    // Update the initial state if it was dependent on this translation..
    if (is_initial_state_dependent_on_in_progress_translation_)
      initial_state_is_translated_ = previous_state_is_translated_;

    // Check if this was the first error, and then increment the number of
    // errors for this page load.
    if (first_translate_error_type_ == TranslateErrors::NONE)
      first_translate_error_type_ = error_type;
    num_translate_errors_++;
  }

  is_translation_in_progress_ = false;
  is_initial_state_dependent_on_in_progress_translation_ = false;
}

void TranslateMetricsLoggerImpl::LogReversion() {
  UpdateTimeTranslated(current_state_is_translated_, is_foreground_);
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

void TranslateMetricsLoggerImpl::LogInitialSourceLanguage(
    const std::string& source_language_code,
    bool is_in_users_content_languages) {
  initial_source_language_ = source_language_code;
  is_initial_source_language_in_users_content_languages_ =
      is_in_users_content_languages;

  current_source_language_ = source_language_code;
}

void TranslateMetricsLoggerImpl::LogSourceLanguage(
    const std::string& source_language_code) {
  current_source_language_ = source_language_code;
}

void TranslateMetricsLoggerImpl::LogTargetLanguage(
    const std::string& target_language_code) {
  if (initial_target_language_ == "")
    initial_target_language_ = target_language_code;

  // Only increment |num_target_language_changes_| if |current_target_language_|
  // changes between two languages.
  if (current_target_language_ != "" &&
      current_target_language_ != target_language_code)
    num_target_language_changes_++;

  current_target_language_ = target_language_code;
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

void TranslateMetricsLoggerImpl::UpdateTimeTranslated(bool was_translated,
                                                      bool was_foreground) {
  base::TimeTicks current_time = clock_->NowTicks();
  if (was_foreground) {
    base::TimeDelta time_since_last_update =
        current_time - time_of_last_state_change_;
    if (was_translated)
      total_time_translated_ += time_since_last_update;
    else
      total_time_not_translated_ += time_since_last_update;
  }
  time_of_last_state_change_ = current_time;
}

void TranslateMetricsLoggerImpl::SetInternalClockForTesting(
    base::TickClock* clock) {
  clock_ = clock;
  time_of_last_state_change_ = clock_->NowTicks();
}

}  // namespace translate
