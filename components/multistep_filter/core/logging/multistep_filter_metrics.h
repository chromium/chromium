// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_

#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"

namespace multistep_filter {

// Suffix for histograms that are broken down by task type.
inline constexpr char kMultistepFilterByTaskHistogramPrefix[] = ".ByTask.";

// Histogram names and prefixes for Multistep Filter metrics.
inline constexpr char kMultistepFilterAcceptanceHistogram[] =
    "MultistepFilter.Acceptance";
inline constexpr char kMultistepFilterAcceptanceInitialCueHistogram[] =
    "MultistepFilter.Acceptance.InitialCue";
inline constexpr char kMultistepFilterAcceptanceReopenedCueHistogram[] =
    "MultistepFilter.Acceptance.ReopenedCue";

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_METRICS_H_
