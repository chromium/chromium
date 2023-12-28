// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_METRICS_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_METRICS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/omnibox/browser/autocomplete_result.h"

class AutocompleteController;
class AutocompleteProvider;

// Used to track and log timing metrics for `AutocompleteController`. Logs 3
// sets of metrics:
// 1) How long until each async provider completes.
//    - Does not track intermediate updates if an async provider updates results
//      multiple times.
//    - Does not track sync providers (i.e., providers that don't use
//      `AutocompleteProvider::NotifyListeners()` to notify the
//      `AutocompleteController`.
//    - Tracks async providers completing syncly (i.e., providers that invoke
//      `AutocompleteProvider::NotifyListeners()` syncly; see the comment in
//      `AutocompleteController::OnProviderUpdate()`).
// 2) How long until the suggestions finalize.
//    - Does not track sync requests (i.e.,
//      `AutocompleteInput::omit_asynchronous_matches()` set to true).
//    - Does track async requests that complete syncly.
//    - Tracks suggestion additions, changes, and removals.
// 3) How many suggestions change during updates.
//    - Does not track sync request.
//    - Tracks both sync and async updates.
//    - Does not track suggestion removals.
//    - Tracks suggestion additions and changes.
class AutocompleteControllerMetrics {
 public:
  explicit AutocompleteControllerMetrics(
      const AutocompleteController& controller);

  // Called when `AutocompleteController::Start()` is called. Will 1) log
  // suggestion finalization metrics for the previous request if it hasn't
  // already (i.e. if it hasn't completed and is being interrupted), and 2)
  // reset state to track the new request. Should be called before
  // `AutocompleteController::done()`,
  // `::expire_timer_done()`, or `AutocompleteProvider::done()` have been
  // updated for the new request.
  void OnStart();

  // Called when `AutocompleteController::NotifyChanged()` is called. Will log
  // metrics on how many suggestions changed with this update. If the controller
  // is done, will also log suggestion finalization metrics; otherwise, future
  // calls to `OnProviderUpdate()`, `OnStop()`, or `OnStart()` will log
  // suggestion finalization metrics.
  void OnNotifyChanged(
      std::vector<AutocompleteResult::MatchDedupComparator> last_result,
      std::vector<AutocompleteResult::MatchDedupComparator> new_result);

  // Called when `AutocompleteController::OnProviderUpdate()` is called. If the
  // provider is done, will log how long it took; otherwise, future calls to
  // `OnProviderUpdate()`, `OnStop()`, or `OnStart()` will log how long the
  // provider took.
  void OnProviderUpdate(const AutocompleteProvider& provider) const;

  // Called when either `AutocompleteController::StopHelper()` or `OnStart()`
  // are called; i.e., when the ongoing request, if incomplete, will be
  // interrupted, e.g., because the input was updated, the popup was closed, or
  // the `AutocompleteController::stop_timer_` expired. Should be called before
  // `AutocompleteController::done()`, `::expire_timer_done()`, or
  // `AutocompleteProvider::done()` have been updated.
  void OnStop();

 private:
  friend class AutocompleteControllerMetricsTest;

  // Logs
  // 'Omnibox.AsyncAutocompletionTime.[Done|LastChange|LastDefaultChange]'.
  // Additionally logs either '*.Completed' or '*.Interrupted' for each of the
  // 3 depending on whether the controller completed or was interrupted.
  void LogSuggestionFinalizationMetrics();

  // Logs 'Omnibox.AsyncAutocompletionTime.Provider.<provider name>'.
  // Additionally logs either '*.Completed' or '*.Interrupted' depending
  // whether the provider completed or was interrupted.
  void LogProviderTimeMetrics(const AutocompleteProvider& provider) const;

  // Helper for the above 2 logging methods. Logs
  // 'Omnibox.AsyncAutocompletionTime.<name>'. Additionally logs either
  // '*.Completed' or '*.Interrupted' depending on `completed`.
  void LogAsyncAutocompletionTimeMetrics(const std::string& name,
                                         bool completed,
                                         const base::TimeTicks end_time) const;

  // Logs 'Omnibox.MatchStability.MatchChangeIndex'. Additionally logs
  // '*.CrossInput' or '*.Async' depending on `controller_.in_start()`.
  void LogSuggestionChangeIndexMetrics(size_t change_index) const;

  // Logs 'Omnibox.MatchStability.MatchChangeInAnyPosition'. Additionally logs
  // '*.CrossInput' or '*.Async' depending on `controller_.in_start()`.
  void LogSuggestionChangeInAnyPositionMetrics(bool changed) const;

  const raw_ref<const AutocompleteController> controller_;

  // When `OnStart()` was last invoked. Used for measuring latency. Valid even
  // if `controller_.in_start_` is false.
  base::TimeTicks start_time_;
  // When `OnProviderUpdate()` was last invoked and detected any change to the
  // suggestions.
  base::TimeTicks last_change_time_;
  // When `OnProviderUpdate()` was last invoked and detected a change to the
  // default suggestion.
  base::TimeTicks last_default_change_time_;
  // Whether `LogSuggestionFinalizationMetrics()` has been invoked for the
  // current request. Used for `DCHECK`s and iOS only. The autocomplete
  // controller state should be the source of truth instead.
  bool logged_finalization_metrics_ = true;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_METRICS_H_
