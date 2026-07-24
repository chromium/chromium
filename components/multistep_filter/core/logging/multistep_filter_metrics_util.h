// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_UTIL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_UTIL_H_

#include <string_view>

#include "base/functional/function_ref.h"
#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"

namespace multistep_filter {

struct RetentionStateSnapshot;

// Returns true if the suggestion is on the same etld+1 as the triggering
// navigation.
bool IsSameEtldPlusOne(const UrlFilterSuggestion& suggestion);

// Returns the clamped difference between two TimeTicks.
base::TimeDelta GetClampedDifference(base::TimeTicks end,
                                     base::TimeTicks start);

// Returns the clamped difference between two Times.
base::TimeDelta GetClampedDifference(base::Time end, base::Time start);

// Returns the MultistepFilterTaskType enum for the given task type string.
MultistepFilterTaskType MapStringToTaskType(std::string_view task_type);

// Returns the MultistepFilterFacetType enum for the given filter facet string.
MultistepFilterFacetType MapStringToFacetType(std::string_view filter_facet);

// Returns the retention state for a given snapshot.
MultistepFilterRetentionState GetRetentionState(
    const RetentionStateSnapshot& snapshot);

// Calls the callback for each retention slice string associated with the state.
void EnumerateActiveRetentionSlices(
    MultistepFilterRetentionState state,
    base::FunctionRef<void(std::string_view)> callback);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_UTIL_H_
