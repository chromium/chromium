// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autocomplete_controller_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {

using omnibox_feature_configs::AutocompleteControllerMetricsOptimization;

enum class MetricNameSuffix {
  kDone,
  kLastChange,
  kLastDefaultChange,
  kProvider,
  kMaxValue = kProvider
};

constexpr std::string_view ToString(MetricNameSuffix name_suffix) {
  using enum MetricNameSuffix;
  switch (name_suffix) {
    case kDone:
      return "Done";
    case kLastChange:
      return "LastChange";
    case kLastDefaultChange:
      return "LastDefaultChange";
    case kProvider:
      return "Provider";
  }
  NOTREACHED();
}

std::string GetMetricName(MetricNameSuffix name_suffix,
                          const AutocompleteProvider* provider,
                          std::string_view completion_suffix = "") {
  return base::StrCat(
      {"Omnibox.AsyncAutocompletionTime2.", ToString(name_suffix),
       (provider ? "." : ""), (provider ? provider->GetName() : ""),
       (completion_suffix.empty() ? "" : "."), completion_suffix});
}

void LogAsyncAutocompletionTimeMetricsImpl(MetricNameSuffix name_suffix,
                                           const AutocompleteProvider* provider,
                                           bool completed,
                                           base::TimeDelta elapsed_time) {
  // This may be called up to 40 times per omnibox key-stroke. Cache the
  // histograms in a lookup table keyed  by name_suffix + provider_number
  // (where provider_number is 0 unless name_suffix == kProvider) so that
  // we can avoid having to construct their names and look them up each time.

  // The max size of each of the histogram tables.
  constexpr int kMaxHistogramSize =
      (static_cast<int>(MetricNameSuffix::kMaxValue) +
       metrics::OmniboxEventProto_ProviderType_ProviderType_MAX) +
      1;

  // Validate the histogram lookup parameters:
  // * name_suffix is in [0..kMaxValue]
  // * `provider` is non-null iff name_suffix == kProvider, and vice versa.
  // * if non-null, provider yields a value <= ProviderType_MAX
  DCHECK_GE(static_cast<int>(name_suffix), 0);
  DCHECK_LE(static_cast<int>(name_suffix),
            static_cast<int>(MetricNameSuffix::kMaxValue));
  DCHECK_EQ(name_suffix == MetricNameSuffix::kProvider, !!provider);
  DCHECK(!provider ||
         provider->AsOmniboxEventProviderType() <=
             metrics::OmniboxEventProto_ProviderType_ProviderType_MAX);

  // Each histogram is at the same index in its respective table.
  const int histogram_index =
      static_cast<int>(name_suffix) +
      (provider ? provider->AsOmniboxEventProviderType() : 0);

  // Use the `STATIC_HISTOGRAM_POINTER_GROUP` macro to define a static table of
  // atomic histogram pointers which is indexed by `histogram_index`.
  //
  // I.e., the histograms are ordered as:
  //   Done, LastChange, DefaultChange, Provider-0, Provider-1, ...
#define STATIC_HISTOGRAM_TIMES_POINTER_GROUP(name, sample)    \
  STATIC_HISTOGRAM_POINTER_GROUP(                             \
      name, histogram_index, kMaxHistogramSize,               \
      AddTimeMillisecondsGranularity(sample),                 \
      base::Histogram::FactoryTimeGet(                        \
          name, base::Milliseconds(1), base::Seconds(10), 50, \
          base::HistogramBase::kUmaTargetedHistogramFlag))

  // These metrics are logged up to about 40 times each per omnibox keystroke.
  // `GetMetricName()` is deterministic for any given set of parameters, so each
  // histogram name is a run-time constant and a pointer to the corresponding
  // histogram object will be cached on first use in a static table.
  STATIC_HISTOGRAM_TIMES_POINTER_GROUP(GetMetricName(name_suffix, provider),
                                       elapsed_time);
  if (completed) {
    STATIC_HISTOGRAM_TIMES_POINTER_GROUP(
        GetMetricName(name_suffix, provider, "Completed"), elapsed_time);
  } else {
    STATIC_HISTOGRAM_TIMES_POINTER_GROUP(
        GetMetricName(name_suffix, provider, "Interrupted"), elapsed_time);
  }
#undef STATIC_HISTOGRAM_TIMES_POINTER_GROUP
}

inline void LogAsyncAutocompletionTimeMetrics(MetricNameSuffix metric,
                                              bool completed,
                                              base::TimeDelta elapsed_time) {
  DCHECK_NE(metric, MetricNameSuffix::kProvider);
  LogAsyncAutocompletionTimeMetricsImpl(metric, nullptr, completed,
                                        elapsed_time);
}

inline void LogAsyncAutocompletionTimeMetrics(
    const AutocompleteProvider& provider,
    base::TimeDelta elapsed_time) {
  LogAsyncAutocompletionTimeMetricsImpl(MetricNameSuffix::kProvider, &provider,
                                        provider.done(), elapsed_time);
}

void OldLogAsyncAutocompletionTimeMetrics(const std::string& name,
                                          bool completed,
                                          base::TimeDelta elapsed_time) {
  const auto name_prefix = "Omnibox.AsyncAutocompletionTime2." + name;

  // These metrics are logged up to about 40 times per omnibox keystroke. Use
  // the, less efficient, UMA histogram functions because the names are dynamic.
  // Each histogram function invocation results in a string alloc and a
  // histogram lookup => ~120 allocs and 120 lookups per omnibox keystroke.
  base::UmaHistogramTimes(name_prefix, elapsed_time);
  if (completed) {
    base::UmaHistogramTimes(name_prefix + ".Completed", elapsed_time);
  } else {
    base::UmaHistogramTimes(name_prefix + ".Interrupted", elapsed_time);
  }
}

}  // namespace

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
  if (controller_->input().omit_asynchronous_matches()) {
    return;
  }

  // If results are empty then the omnibox is likely closed, and clearing old
  // results won't be user visible. E.g., this occurs when opening a new tab
  // while the popup was open.
  if (new_result.empty()) {
    return;
  }

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

  // Log suggestion finalization times. This handles logging as soon as the
  // final update occurs, while `OnStop()` handles the case where the final
  // update never occurs because of interruptions.

  // E.g., suggestion deletion can call `OnNotifyChanged()` after the controller
  // is done and finalization metrics have been logged. They should not be
  // logged again.
  if (controller_->last_update_type() ==
      AutocompleteController::UpdateType::kMatchDeletion) {
    return;
  }

  const bool any_match_changed_or_removed_or_added =
      any_match_changed_or_removed || last_result.size() != new_result.size();
  const bool default_match_changed_or_removed_or_added =
      last_result.empty() || last_result[0] != new_result[0];

  if (any_match_changed_or_removed_or_added) {
    last_change_time_ = base::TimeTicks::Now();
  }
  if (default_match_changed_or_removed_or_added) {
    DCHECK(any_match_changed_or_removed_or_added);
    last_default_change_time_ = last_change_time_;
  }
  // It's common to have multiple async updates per input. Only log the final
  // update.
  // TODO(crbug.com/364303536): `logged_finalization_metrics_` should be
  //   guaranteed false here (hence the DCHECK in
  //   `LogSuggestionFinalizationMetrics()`. But because of a temporary band-aid
  //   to allow history embedding answers and unscoped extension suggestions to
  //   ignore the stop timer, we need to check it anyways.
  if (controller_->done() && !logged_finalization_metrics_) {
    LogSuggestionFinalizationMetrics();
  }
}

void AutocompleteControllerMetrics::OnProviderUpdate(
    const AutocompleteProvider& provider) const {
  // Only log metrics for async requests. This will likely never happen, since
  // `OnProviderUpdate()` is only called by async providers (but not necessarily
  // async'ly, see the comments in `AutocompleteController::OnProviderUpdate`).
  if (controller_->input().omit_asynchronous_matches()) {
    return;
  }

  // Some async providers may produce multiple updates. Only log the final async
  // update.
  if (provider.done()) {
    LogProviderTimeMetrics(provider);
  }
}

void AutocompleteControllerMetrics::OnStop() {
  // Only log metrics for async requests.
  if (controller_->input().omit_asynchronous_matches()) {
    return;
  }

  // Done providers should already be logged by `OnProviderUpdate()`.
  for (const auto& provider : controller_->providers()) {
    if (!provider->done()) {
      LogProviderTimeMetrics(*provider);
    }
  }

  // If the controller is done, `OnNotifyChanged()` should have already
  // logged finalization metrics. This case, i.e. `OnStop()` invoked even
  // though the controller is done, is possible because `OnStart()` calls
  // `OnStop()`.
  // TODO(crbug.com/364303536): `logged_finalization_metrics_` should be
  //   guaranteed false here (hence the DCHECK in
  //   `LogSuggestionFinalizationMetrics()`. But because of a temporary
  //   bandaid to allow history embedding answers and unscoped extension
  //   answers to ignore the stop timer, we need to check it anyways.
  if (!controller_->done() && !logged_finalization_metrics_) {
    LogSuggestionFinalizationMetrics();
  }
}

void AutocompleteControllerMetrics::LogSuggestionFinalizationMetrics() {
  // Finalization metrics should be logged once only, either when all
  // async providers complete or they're interrupted before completion.
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

  const auto done_elapsed_time = base::TimeTicks::Now() - start_time_;
  const auto last_change_elapsed_time = last_change_time_ - start_time_;
  const auto last_default_change_elapsed_time =
      last_default_change_time_ - start_time_;
  const bool is_completed = controller_->done();

  if (AutocompleteControllerMetricsOptimization::Get().enabled) {
    using enum MetricNameSuffix;
    LogAsyncAutocompletionTimeMetrics(kDone, is_completed, done_elapsed_time);
    LogAsyncAutocompletionTimeMetrics(kLastChange, is_completed,
                                      last_change_elapsed_time);
    LogAsyncAutocompletionTimeMetrics(kLastDefaultChange, is_completed,
                                      last_default_change_elapsed_time);
  } else {
    OldLogAsyncAutocompletionTimeMetrics("Done", is_completed,
                                         done_elapsed_time);
    OldLogAsyncAutocompletionTimeMetrics("LastChange", is_completed,
                                         last_change_elapsed_time);
    OldLogAsyncAutocompletionTimeMetrics("LastDefaultChange", is_completed,
                                         last_default_change_elapsed_time);
  }
}

void AutocompleteControllerMetrics::LogProviderTimeMetrics(
    const AutocompleteProvider& provider) const {
  const auto elapsed_time = base::TimeTicks::Now() - start_time_;
  if (AutocompleteControllerMetricsOptimization::Get().enabled) {
    LogAsyncAutocompletionTimeMetrics(provider, elapsed_time);
  } else {
    OldLogAsyncAutocompletionTimeMetrics(
        std::string("Provider.") + provider.GetName(), provider.done(),
        elapsed_time);
  }
}

void AutocompleteControllerMetrics::LogSuggestionChangeIndexMetrics(
    size_t change_index) const {
  // These metrics are logged up to about 50 times per omnibox keystroke, so use
  // the UMA macros (which cache the histogram pointer) for efficiency.
  static constexpr char kName[] = "Omnibox.MatchStability2.MatchChangeIndex";
  constexpr size_t max = AutocompleteResult::kMaxAutocompletePositionValue;
  const bool is_sync = (controller_->last_update_type() ==
                            AutocompleteController::UpdateType::kSyncPass ||
                        controller_->last_update_type() ==
                            AutocompleteController::UpdateType::kSyncPassOnly);

  if (AutocompleteControllerMetricsOptimization::Get().enabled) {
    UMA_HISTOGRAM_EXACT_LINEAR(kName, change_index, max);
    if (is_sync) {
      UMA_HISTOGRAM_EXACT_LINEAR(base::StrCat({kName, ".CrossInput"}),
                                 change_index, max);
    } else {
      UMA_HISTOGRAM_EXACT_LINEAR(base::StrCat({kName, ".Async"}), change_index,
                                 max);
    }
  } else {
    const std::string name = kName;  // Unnecessary string alloc!
    UMA_HISTOGRAM_EXACT_LINEAR(name, change_index, max);
    if (is_sync) {
      UMA_HISTOGRAM_EXACT_LINEAR(name + ".CrossInput", change_index, max);
    } else {
      UMA_HISTOGRAM_EXACT_LINEAR(name + ".Async", change_index, max);
    }
  }
}

void AutocompleteControllerMetrics::LogSuggestionChangeInAnyPositionMetrics(
    bool changed) const {
  // These metrics are logged up to about 5 times per omnibox keystroke, so use
  // the UMA macros (which cache the histogram pointer) for efficiency.
  static constexpr char kName[] =
      "Omnibox.MatchStability2.MatchChangeInAnyPosition";
  const bool is_sync = (controller_->last_update_type() ==
                            AutocompleteController::UpdateType::kSyncPass ||
                        controller_->last_update_type() ==
                            AutocompleteController::UpdateType::kSyncPassOnly);
  if (AutocompleteControllerMetricsOptimization::Get().enabled) {
    UMA_HISTOGRAM_BOOLEAN(kName, changed);
    if (is_sync) {
      UMA_HISTOGRAM_BOOLEAN(base::StrCat({kName, ".CrossInput"}), changed);
    } else {
      UMA_HISTOGRAM_BOOLEAN(base::StrCat({kName, ".Async"}), changed);
    }
  } else {
    const std::string name = kName;  // Unnecessary string alloc!
    UMA_HISTOGRAM_BOOLEAN(name, changed);
    if (is_sync) {
      UMA_HISTOGRAM_BOOLEAN(name + ".CrossInput", changed);
    } else {
      UMA_HISTOGRAM_BOOLEAN(name + ".Async", changed);
    }
  }
}
