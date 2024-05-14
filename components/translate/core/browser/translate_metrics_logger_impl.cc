// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace translate {

// Translation frequency UMA histograms.
const char kTranslateTranslationSourceLanguage[] =
    "Translate.Translation.SourceLanguage";
const char kTranslateTranslationStatus[] = "Translate.Translation.Status";
const char kTranslateTranslationTargetLanguage[] =
    "Translate.Translation.TargetLanguage";
const char kTranslateTranslationTargetLanguageOrigin[] =
    "Translate.Translation.TargetLanguage.Origin";
const char kTranslateTranslationType[] = "Translate.Translation.Type";

// UI Interaction frequency UMA histograms.
const char kTranslateUiInteractionEvent[] = "Translate.UiInteraction.Event";

// Page-load frequency UMA histograms.
const char kTranslatePageLoadFinalSourceLanguage[] =
    "Translate.PageLoad.FinalSourceLanguage";
const char kTranslatePageLoadFinalState[] = "Translate.PageLoad.FinalState";
const char kTranslatePageLoadFinalTargetLanguage[] =
    "Translate.PageLoad.FinalTargetLanguage";
const char kTranslatePageLoadHrefTriggerDecision[] =
    "Translate.PageLoad.HrefHint.TriggerDecision";
const char kTranslatePageLoadInitialSourceLanguage[] =
    "Translate.PageLoad.InitialSourceLanguage";
const char kTranslatePageLoadInitialState[] = "Translate.PageLoad.InitialState";
const char kTranslatePageLoadInitialTargetLanguage[] =
    "Translate.PageLoad.InitialTargetLanguage";
const char kTranslatePageLoadInitialTargetLanguageOrigin[] =
    "Translate.PageLoad.InitialTargetLanguage.Origin";
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
const char kTranslatePageLoadRankerTimerShouldOfferTranslation[] =
    "Translate.PageLoad.Ranker.Timer.ShouldOffereTranslation";
const char kTranslatePageLoadRankerVersion[] =
    "Translate.PageLoad.Ranker.Version";
const char kTranslatePageLoadTriggerDecision[] =
    "Translate.PageLoad.TriggerDecision";

// Application frequency UMA histograms.
const char kTranslateApplicationStartAlwaysTranslateLanguage[] =
    "Translate.ApplicationStart.AlwaysTranslateLanguage";
const char kTranslateApplicationStartAlwaysTranslateLanguageCount[] =
    "Translate.ApplicationStart.AlwaysTranslateLanguage.Count";
const char kTranslateApplicationStartNeverTranslateLanguage[] =
    "Translate.ApplicationStart.NeverTranslateLanguage";
const char kTranslateApplicationStartNeverTranslateLanguageCount[] =
    "Translate.ApplicationStart.NeverTranslateLanguage.Count";
const char kTranslateApplicationStartNeverTranslateSiteCount[] =
    "Translate.ApplicationStart.NeverTranslateSite.Count";

TranslationType NullTranslateMetricsLogger::GetNextManualTranslationType(
    bool is_context_menu_initiated_translation) {
  return TranslationType::kUninitialized;
}

void TranslateMetricsLoggerImpl::LogApplicationStartMetrics(
    std::unique_ptr<TranslatePrefs> translate_prefs) {
  // TODO(crbug.com/40778331): These histograms are only recorded when Chrome
  // starts up using the preferences of whatever profile is logged in at the
  // time. This information should be recorded each time a profile logs in.

  std::vector<std::string> always_translate_languages =
      translate_prefs->GetAlwaysTranslateLanguages();
  for (const auto& always_translate_language : always_translate_languages) {
    base::UmaHistogramSparse(kTranslateApplicationStartAlwaysTranslateLanguage,
                             base::HashMetricName(always_translate_language));
  }
  base::UmaHistogramCounts100(
      kTranslateApplicationStartAlwaysTranslateLanguageCount,
      always_translate_languages.size());

  std::vector<std::string> never_translate_languages =
      translate_prefs->GetNeverTranslateLanguages();
  for (const auto& never_translate_language : never_translate_languages) {
    base::UmaHistogramSparse(kTranslateApplicationStartNeverTranslateLanguage,
                             base::HashMetricName(never_translate_language));
  }
  base::UmaHistogramCounts100(
      kTranslateApplicationStartNeverTranslateLanguageCount,
      never_translate_languages.size());

  std::vector<std::string> never_translate_sites =
      translate_prefs->GetNeverPromptSitesBetween(base::Time::UnixEpoch(),
                                                  base::Time::Now());
  base::UmaHistogramCounts100(kTranslateApplicationStartNeverTranslateSiteCount,
                              never_translate_sites.size());
}

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

  // If the status of the most recent translation hasn't be reported yet, report
  // it as a "success".
  if (is_translation_status_pending_)
    RecordTranslationStatus(ConvertTranslationTypeToSuccessfulTranslationStatus(
        is_translation_in_progress_, current_translation_type_));

  is_translation_status_pending_ = false;
  current_translation_type_ = TranslationType::kUninitialized;

  // If a translation is still in progress, then use the previous state.
  bool this_initial_state_is_translated =
      is_initial_state_dependent_on_in_progress_translation_
          ? previous_state_is_translated_
          : initial_state_is_translated_;
  bool this_current_state_is_translated = is_translation_in_progress_
                                              ? previous_state_is_translated_
                                              : current_state_is_translated_;

  // The first time |RecordMetrics| is called, record all page load frequency
  // UMA metrcis.
  if (sequence_no_ == 0) {
    RecordPageLoadUmaMetrics(this_initial_state_is_translated,
                             this_current_state_is_translated);
  }

  // Record metrics to UKM.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::TranslatePageLoad(ukm_source_id_)
      .SetSequenceNumber(sequence_no_)
      .SetTriggerDecision(int(trigger_decision_))
      .SetRankerDecision(int(ranker_decision_))
      .SetRankerVersion(int(ranker_version_))
      .SetInitialState(int(ConvertToTranslateState(
          this_initial_state_is_translated, initial_state_is_ui_shown_,
          initial_state_is_omnibox_icon_shown_)))
      .SetFinalState(int(ConvertToTranslateState(
          this_current_state_is_translated, current_state_is_ui_shown_,
          current_state_is_omnibox_icon_shown_)))
      .SetNumTranslations(
          ukm::GetExponentialBucketMinForCounts1000(num_translations_))
      .SetNumReversions(
          ukm::GetExponentialBucketMinForCounts1000(num_reversions_))
      .SetInitialSourceLanguage(
          int(base::HashMetricName(initial_source_language_)))
      .SetFinalSourceLanguage(
          int(base::HashMetricName(current_source_language_)))
      .SetInitialSourceLanguageInContentLanguages(
          int(is_initial_source_language_in_users_content_languages_))
      .SetInitialTargetLanguage(
          int(base::HashMetricName(initial_target_language_)))
      .SetFinalTargetLanguage(
          int(base::HashMetricName(current_target_language_)))
      .SetNumTargetLanguageChanges(ukm::GetExponentialBucketMinForCounts1000(
          num_target_language_changes_))
      .SetFirstUIInteraction(int(first_ui_interaction_))
      .SetNumUIInteractions(
          ukm::GetExponentialBucketMinForCounts1000(num_ui_interactions_))
      .SetFirstTranslateError(int(first_translate_error_type_))
      .SetNumTranslateErrors(
          ukm::GetExponentialBucketMinForCounts1000(num_translate_errors_))
      .SetTotalTimeTranslated(ukm::GetExponentialBucketMinForUserTiming(
          total_time_translated_.InSeconds()))
      .SetTotalTimeNotTranslated(ukm::GetExponentialBucketMinForUserTiming(
          total_time_not_translated_.InSeconds()))
      .SetMaxTimeToTranslate(ukm::GetExponentialBucketMinForUserTiming(
          max_time_to_translate_.InMilliseconds()))
      .SetModelDetectionReliabilityScore(ukm::GetLinearBucketMin(
          static_cast<int64_t>(100 * model_detection_reliability_score_), 5))
      .SetModelDetectedLanguage(
          int(base::HashMetricName(model_detected_language_)))
      .SetHTMLContentLanguage(int(base::HashMetricName(html_content_language_)))
      .SetHTMLDocumentLanguage(int(base::HashMetricName(html_doc_language_)))
      .SetWasContentEmpty(was_content_empty_)
      .Record(ukm_recorder);

  sequence_no_++;
}

void TranslateMetricsLoggerImpl::SetUkmSourceId(ukm::SourceId ukm_source_id) {
  ukm_source_id_ = ukm_source_id;
}

void TranslateMetricsLoggerImpl::RecordPageLoadUmaMetrics(
    bool initial_state_is_translated,
    bool current_state_is_translated) {
  base::UmaHistogramEnumeration(kTranslatePageLoadRankerDecision,
                                ranker_decision_);
  base::UmaHistogramSparse(kTranslatePageLoadRankerVersion,
                           int(ranker_version_));
  if (ranker_duration_)
    base::UmaHistogramTimes(kTranslatePageLoadRankerTimerShouldOfferTranslation,
                            ranker_duration_.value());

  base::UmaHistogramEnumeration(kTranslatePageLoadTriggerDecision,
                                trigger_decision_);
  if (has_href_translate_target_) {
    base::UmaHistogramEnumeration(kTranslatePageLoadHrefTriggerDecision,
                                  trigger_decision_);
  }
  base::UmaHistogramEnumeration(
      kTranslatePageLoadInitialState,
      ConvertToTranslateState(initial_state_is_translated,
                              initial_state_is_ui_shown_,
                              initial_state_is_omnibox_icon_shown_));
  base::UmaHistogramEnumeration(
      kTranslatePageLoadFinalState,
      ConvertToTranslateState(current_state_is_translated,
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
  base::UmaHistogramEnumeration(kTranslatePageLoadInitialTargetLanguageOrigin,
                                initial_target_language_origin_);
}

void TranslateMetricsLoggerImpl::RecordTranslationHistograms(
    TranslationType translation_type,
    const std::string& source_language,
    const std::string& target_language,
    TranslateBrowserMetrics::TargetLanguageOrigin target_language_origin) {
  base::UmaHistogramEnumeration(kTranslateTranslationType, translation_type);
  base::UmaHistogramSparse(kTranslateTranslationSourceLanguage,
                           base::HashMetricName(source_language));
  base::UmaHistogramSparse(kTranslateTranslationTargetLanguage,
                           base::HashMetricName(target_language));
  base::UmaHistogramEnumeration(kTranslateTranslationTargetLanguageOrigin,
                                target_language_origin);
}

void TranslateMetricsLoggerImpl::RecordTranslationStatus(
    TranslationStatus translation_status) {
  base::UmaHistogramEnumeration(kTranslateTranslationStatus,
                                translation_status);
}

void TranslateMetricsLoggerImpl::LogRankerMetrics(
    RankerDecision ranker_decision,
    uint32_t ranker_version) {
  ranker_decision_ = ranker_decision;
  ranker_version_ = ranker_version;
}

void TranslateMetricsLoggerImpl::LogRankerStart() {
  if (!ranker_duration_)
    ranker_start_time_ = clock_->NowTicks();
}

void TranslateMetricsLoggerImpl::LogRankerFinish() {
  if (!ranker_duration_)
    ranker_duration_ = clock_->NowTicks() - ranker_start_time_;
}

void TranslateMetricsLoggerImpl::LogTriggerDecision(
    TriggerDecision trigger_decision) {
  // Only stores the first non-kUninitialized trigger decision that is logged,
  // except in the case that Href translate overrides the decision to either
  // auto translate or show the UI, while autotranslation to a predefined
  // target similarly overrides everything except autotranslation by Href.
  if (trigger_decision_ == TriggerDecision::kUninitialized ||
      trigger_decision == TriggerDecision::kAutomaticTranslationByHref ||
      (trigger_decision == TriggerDecision::kShowUIFromHref &&
       trigger_decision_ != TriggerDecision::kAutomaticTranslationByHref) ||
      (trigger_decision ==
           TriggerDecision::kAutomaticTranslationToPredefinedTarget &&
       trigger_decision_ != TriggerDecision::kAutomaticTranslationByHref)) {
    trigger_decision_ = trigger_decision;
  }
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

void TranslateMetricsLoggerImpl::LogTranslationStarted(
    TranslationType translation_type) {
  if (is_translation_status_pending_)
    RecordTranslationStatus(TranslationStatus::kNewTranslation);

  // Save the previous state in case the translation fails.
  previous_state_is_translated_ = current_state_is_translated_;

  current_state_is_translated_ = true;
  is_translation_in_progress_ = true;
  is_translation_status_pending_ = true;
  current_translation_type_ = translation_type;
  has_any_translation_started_ = true;

  time_of_last_translation_start_ = clock_->NowTicks();

  RecordTranslationHistograms(
      current_translation_type_, current_source_language_,
      current_target_language_, current_target_language_origin_);
}

void TranslateMetricsLoggerImpl::LogTranslationFinished(
    bool was_successful,
    TranslateErrors error_type) {
  // Note that a translation can fail (i.e. was_successful is false) and have an
  // error type of NONE in some cases. One case where this happens is when a
  // translation is interrupted midway through.
  if (was_successful) {
    UpdateTimeTranslated(previous_state_is_translated_, is_foreground_);
    num_translations_++;

    // Calculate the time it took to complete this translation, and check if is
    // the the longest translation for this page load.
    base::TimeDelta time_of_translation =
        clock_->NowTicks() - time_of_last_translation_start_;
    if (time_of_translation > max_time_to_translate_)
      max_time_to_translate_ = time_of_translation;
  } else {
    // If the translation fails, then undo the change to the current state.
    current_state_is_translated_ = previous_state_is_translated_;

    // Update the initial state if it was dependent on this translation..
    if (is_initial_state_dependent_on_in_progress_translation_)
      initial_state_is_translated_ = previous_state_is_translated_;

    if (is_translation_status_pending_)
      RecordTranslationStatus(ConvertTranslationTypeToFailedTranslationStatus(
          current_translation_type_, error_type != TranslateErrors::NONE));

    is_translation_status_pending_ = false;
    current_translation_type_ = TranslationType::kUninitialized;
  }

  // If there was some error, checks if this was the first error, and increments
  // the error count.
  if (error_type != TranslateErrors::NONE) {
    if (first_translate_error_type_ == TranslateErrors::NONE)
      first_translate_error_type_ = error_type;
    num_translate_errors_++;
  }

  is_translation_in_progress_ = false;
  is_initial_state_dependent_on_in_progress_translation_ = false;
}

void TranslateMetricsLoggerImpl::LogReversion() {
  UpdateTimeTranslated(current_state_is_translated_, is_foreground_);

  if (is_translation_status_pending_)
    RecordTranslationStatus(ConvertTranslationTypeToRevertedTranslationStatus(
        current_translation_type_));

  current_state_is_translated_ = false;
  is_translation_status_pending_ = false;
  current_translation_type_ = TranslationType::kUninitialized;
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
    const std::string& target_language_code,
    TranslateBrowserMetrics::TargetLanguageOrigin target_language_origin) {
  if (initial_target_language_ == "") {
    initial_target_language_ = target_language_code;
    initial_target_language_origin_ = target_language_origin;
  }

  // Only increment |num_target_language_changes_| if |current_target_language_|
  // changes between two languages.
  if (current_target_language_ != "" &&
      current_target_language_ != target_language_code)
    num_target_language_changes_++;

  current_target_language_ = target_language_code;
  current_target_language_origin_ = target_language_origin;
}

void TranslateMetricsLoggerImpl::LogUIInteraction(
    UIInteraction ui_interaction) {
  if (first_ui_interaction_ == UIInteraction::kUninitialized)
    first_ui_interaction_ = ui_interaction;

  num_ui_interactions_++;

  // Record this UI interaction to the Translate.UiInteraction.Event UMA
  // histogram immediately.
  base::UmaHistogramEnumeration(kTranslateUiInteractionEvent, ui_interaction);
}

TranslationType TranslateMetricsLoggerImpl::GetNextManualTranslationType(
    bool is_context_menu_initiated_translation) {
  if (is_context_menu_initiated_translation) {
    return has_any_translation_started_
               ? TranslationType::kManualContextMenuReTranslation
               : TranslationType::kManualContextMenuInitialTranslation;
  }

  // If the translation was not initiated from the context menu, then it must
  // have been triggered from the translate UI.
  return has_any_translation_started_
             ? TranslationType::kManualUiReTranslation
             : TranslationType::kManualUiInitialTranslation;
}

void TranslateMetricsLoggerImpl::SetHasHrefTranslateTarget(
    bool has_href_translate_target) {
  has_href_translate_target_ = has_href_translate_target;
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

  NOTREACHED_IN_MIGRATION();
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

TranslationStatus
TranslateMetricsLoggerImpl::ConvertTranslationTypeToRevertedTranslationStatus(
    TranslationType translation_type) {
  if (translation_type == TranslationType::kManualUiInitialTranslation ||
      translation_type == TranslationType::kManualUiReTranslation)
    return TranslationStatus::kRevertedManualUiTranslation;
  if (translation_type ==
          TranslationType::kManualContextMenuInitialTranslation ||
      translation_type == TranslationType::kManualContextMenuReTranslation)
    return TranslationStatus::kRevertedManualContextMenuTranslation;
  if (translation_type == TranslationType::kAutomaticTranslationByPref ||
      translation_type == TranslationType::kAutomaticTranslationByLink) {
    return TranslationStatus::kRevertedAutomaticTranslation;
  }
  if (translation_type ==
      TranslationType::kAutomaticTranslationToPredefinedTarget) {
    return TranslationStatus::kRevertedAutomaticTranslationToPredefinedTarget;
  }
  if (translation_type == TranslationType::kAutomaticTranslationByHref) {
    return TranslationStatus::kRevertedAutomaticTranslationByHref;
  }
  return TranslationStatus::kUninitialized;
}

TranslationStatus
TranslateMetricsLoggerImpl::ConvertTranslationTypeToFailedTranslationStatus(
    TranslationType translation_type,
    bool was_translation_error) {
  if (translation_type == TranslationType::kManualUiInitialTranslation ||
      translation_type == TranslationType::kManualUiReTranslation) {
    if (was_translation_error)
      return TranslationStatus::kFailedWithErrorManualUiTranslation;
    else
      return TranslationStatus::kFailedWithNoErrorManualUiTranslation;
  }
  if (translation_type ==
          TranslationType::kManualContextMenuInitialTranslation ||
      translation_type == TranslationType::kManualContextMenuReTranslation) {
    if (was_translation_error)
      return TranslationStatus::kFailedWithErrorManualContextMenuTranslation;
    else
      return TranslationStatus::kFailedWithNoErrorManualContextMenuTranslation;
  }
  if (translation_type == TranslationType::kAutomaticTranslationByPref ||
      translation_type == TranslationType::kAutomaticTranslationByLink) {
    if (was_translation_error)
      return TranslationStatus::kFailedWithErrorAutomaticTranslation;
    else
      return TranslationStatus::kFailedWithNoErrorAutomaticTranslation;
  }
  if (translation_type ==
      TranslationType::kAutomaticTranslationToPredefinedTarget) {
    if (was_translation_error) {
      return TranslationStatus::
          kFailedWithErrorAutomaticTranslationToPredefinedTarget;
    } else {
      return TranslationStatus::
          kFailedWithNoErrorAutomaticTranslationToPredefinedTarget;
    }
  }
  if (translation_type == TranslationType::kAutomaticTranslationByHref) {
    if (was_translation_error) {
      return TranslationStatus::kFailedWithErrorAutomaticTranslationByHref;
    } else {
      return TranslationStatus::kFailedWithNoErrorAutomaticTranslationByHref;
    }
  }
  return TranslationStatus::kUninitialized;
}

TranslationStatus
TranslateMetricsLoggerImpl::ConvertTranslationTypeToSuccessfulTranslationStatus(
    bool is_translation_in_progress,
    TranslationType translation_type) {
  if (is_translation_in_progress)
    return TranslationStatus::kTranslationAbandoned;
  if (translation_type == TranslationType::kManualUiInitialTranslation ||
      translation_type == TranslationType::kManualUiReTranslation)
    return TranslationStatus::kSuccessFromManualUiTranslation;
  if (translation_type ==
          TranslationType::kManualContextMenuInitialTranslation ||
      translation_type == TranslationType::kManualContextMenuReTranslation)
    return TranslationStatus::kSuccessFromManualContextMenuTranslation;
  if (translation_type == TranslationType::kAutomaticTranslationByPref)
    return TranslationStatus::kSuccessFromAutomaticTranslationByPref;
  if (translation_type == TranslationType::kAutomaticTranslationByLink)
    return TranslationStatus::kSuccessFromAutomaticTranslationByLink;
  if (translation_type ==
      TranslationType::kAutomaticTranslationToPredefinedTarget) {
    return TranslationStatus::
        kSuccessFromAutomaticTranslationToPredefinedTarget;
  }
  if (translation_type == TranslationType::kAutomaticTranslationByHref) {
    return TranslationStatus::kSuccessFromAutomaticTranslationByHref;
  }
  return TranslationStatus::kUninitialized;
}

void TranslateMetricsLoggerImpl::SetInternalClockForTesting(
    base::TickClock* clock) {
  clock_ = clock;
  time_of_last_state_change_ = clock_->NowTicks();
}

void TranslateMetricsLoggerImpl::LogHTMLDocumentLanguage(
    const std::string& html_doc_language) {
  html_doc_language_ = html_doc_language;
}

void TranslateMetricsLoggerImpl::LogHTMLContentLanguage(
    const std::string& html_content_language) {
  html_content_language_ = html_content_language;
}
void TranslateMetricsLoggerImpl::LogDetectedLanguage(
    const std::string& model_detected_language) {
  model_detected_language_ = model_detected_language;
}

void TranslateMetricsLoggerImpl::LogDetectionReliabilityScore(
    const float& model_detection_reliability_score) {
  model_detection_reliability_score_ = model_detection_reliability_score;
}

void TranslateMetricsLoggerImpl::LogWasContentEmpty(bool was_content_empty) {
  was_content_empty_ = was_content_empty;
}

}  // namespace translate
