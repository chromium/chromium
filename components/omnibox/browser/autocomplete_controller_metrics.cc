// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_controller_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"

AutocompleteControllerMetrics::AutocompleteControllerMetrics(
    const AutocompleteController& controller)
    : controller_(controller) {}

void AutocompleteControllerMetrics::OnStart() {
  OnStop();
  logged_finalization_metrics_ = false;
  start_time_ = base::TimeTicks::Now();
  last_change_time_ = start_time_;
  last_default_change_time_ = start_time_;
}

void AutocompleteControllerMetrics::OnNotifyChanged(
    std::vector<AutocompleteResult::MatchDedupComparator> last_result,
    std::vector<AutocompleteResult::MatchDedupComparator> new_result) {
  // Only log metrics for async requests.
  if (controller_->input().omit_asynchronous_matches())
    return;

  // If results are empty then the omnibox is likely closed, and clearing old
  // results won't be user visible. E.g., this occurs when opening a new tab
  // while the popup was open.
  if (new_result.empty())
    return;

  // Log suggestion changes.

  bool any_match_changed_or_removed = false;
  for (size_t i = 0; i < last_result.size(); ++i) {
    // Log changed or removed matches. Don't log for matches appended to the
    // bottom since that's less disruptive.
    if (i >= new_result.size() || last_result[i] != new_result[i]) {
      LogSuggestionChangeIndexMetrics(i);
      any_match_changed_or_removed = true;
    }
  }
  LogSuggestionChangeInAnyPositionMetrics(any_match_changed_or_removed);

  // Log suggestion finalization times.
  // This handles logging as soon as the final update occurs, while `OnStop()`
  // handles the case where the final update never occurs because of
  // interruptions.

  // E.g., suggestion deletion can call `OnNotifyChanged()` after the controller
  // is done and finalization metrics have been logged. They shouldn't be
  // re-logged.
  if (controller_->last_update_type() ==
      AutocompleteController::UpdateType::kMatchDeletion)
    return;

  const bool any_match_changed_or_removed_or_added =
      any_match_changed_or_removed || last_result.size() != new_result.size();
  const bool default_match_changed_or_removed_or_added =
      last_result.empty() || last_result[0] != new_result[0];

  if (any_match_changed_or_removed_or_added)
    last_change_time_ = base::TimeTicks::Now();
  if (default_match_changed_or_removed_or_added) {
    DCHECK(any_match_changed_or_removed_or_added);
    last_default_change_time_ = last_change_time_;
  }
  // It's common to have multiple async updates per input. Only log the final
  // update.
  if (controller_->done())
    LogSuggestionFinalizationMetrics();
}

void AutocompleteControllerMetrics::OnProviderUpdate(
    const AutocompleteProvider& provider) const {
  // Only log metrics for async requests. This will likely never happen, since
  // `OnProviderUpdate()` is only called by async providers (but not necessarily
  // async'ly, see the comments in
  // `AutocompleteController::OnProviderUpdate()`).
  if (controller_->input().omit_asynchronous_matches())
    return;

  // Some async providers may produce multiple updates. Only log the final async
  // update.
  if (provider.done())
    LogProviderTimeMetrics(provider);
}

void AutocompleteControllerMetrics::OnStop() {
  // Only log metrics for async requests.
  if (controller_->input().omit_asynchronous_matches())
    return;

  // Done providers should already be logged by `OnProviderUpdate()`.
  for (const auto& provider : controller_->providers()) {
    if (!provider->done())
      LogProviderTimeMetrics(*provider);
  }

  // If the controller is done, `OnNotifyChanged()` should have already logged
  // finalization metrics. This case, i.e. `OnStop()` invoked even though the
  // controller is done, is possible because `OnStart()` calls `OnStop()`.
  if (!controller_->done())
    LogSuggestionFinalizationMetrics();
}

void AutocompleteControllerMetrics::LogSuggestionFinalizationMetrics() {
  // Finalization metrics should be logged once only, either when all async
  // providers complete or they're interrupted before completion.
#if BUILDFLAG(IS_IOS)
  // iOS is weird in that it sometimes calls `InjectAdHocMatch()` when the user
  // selects a suggestion, thus changing the results when autocompletion is done
  // and suggestions should be stable.
  if (logged_finalization_metrics_)
    return;
#endif
  DCHECK(!logged_finalization_metrics_)
      << "last_update_type: "
      << AutocompleteController::UpdateTypeToDebugString(
             controller_->last_update_type());
  logged_finalization_metrics_ = true;

  LogAsyncAutocompletionTimeMetrics("Done", controller_->done(),
                                    base::TimeTicks::Now());
  LogAsyncAutocompletionTimeMetrics("LastChange", controller_->done(),
                                    last_change_time_);
  LogAsyncAutocompletionTimeMetrics("LastDefaultChange", controller_->done(),
                                    last_default_change_time_);
}

void AutocompleteControllerMetrics::LogProviderTimeMetrics(
    const AutocompleteProvider& provider) const {
  LogAsyncAutocompletionTimeMetrics(
      std::string("Provider.") + provider.GetName(), provider.done(),
      base::TimeTicks::Now());
}

void AutocompleteControllerMetrics::LogAsyncAutocompletionTimeMetrics(
    const std::string& name,
    bool completed,
    const base::TimeTicks end_time) const {
  const auto name_prefix = "Omnibox.AsyncAutocompletionTime2." + name;
  const auto elapsed_time = end_time - start_time_;
  // These metrics are logged up to about 40 times per omnibox keystroke. But
  // use UMA functions as the names are dynamic.
  base::UmaHistogramTimes(name_prefix, elapsed_time);
  if (completed)
    base::UmaHistogramTimes(name_prefix + ".Completed", elapsed_time);
  else
    base::UmaHistogramTimes(name_prefix + ".Interrupted", elapsed_time);
}

void AutocompleteControllerMetrics::LogSuggestionChangeIndexMetrics(
    size_t change_index) const {
  std::string name = "Omnibox.MatchStability2.MatchChangeIndex";
  size_t max = AutocompleteResult::kMaxAutocompletePositionValue;
  // These metrics are logged up to about 50 times per omnibox keystroke, so use
  // UMA macros for efficiency.
  if (controller_->last_update_type() ==
          AutocompleteController::UpdateType::kSyncPass ||
      controller_->last_update_type() ==
          AutocompleteController::UpdateType::kSyncPassOnly) {
    UMA_HISTOGRAM_EXACT_LINEAR(name + ".CrossInput", change_index, max);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(name + ".Async", change_index, max);
  }
  UMA_HISTOGRAM_EXACT_LINEAR(name, change_index, max);
}

void AutocompleteControllerMetrics::LogSuggestionChangeInAnyPositionMetrics(
    bool changed) const {
  std::string name = "Omnibox.MatchStability2.MatchChangeInAnyPosition";
  // These metrics are logged up to about 5 times per omnibox keystroke, so
  // use UMA macros for efficiency.
  if (controller_->last_update_type() ==
          AutocompleteController::UpdateType::kSyncPass ||
      controller_->last_update_type() ==
          AutocompleteController::UpdateType::kSyncPassOnly) {
    UMA_HISTOGRAM_BOOLEAN(name + ".CrossInput", changed);
  } else {
    UMA_HISTOGRAM_BOOLEAN(name + ".Async", changed);
  }
  UMA_HISTOGRAM_BOOLEAN(name, changed);
}
