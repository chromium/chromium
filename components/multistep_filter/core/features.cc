// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/features.h"

#include <cstddef>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace multistep_filter {

// Enables the Multistep Filter feature to generate filter suggestions to users
// based on their previous browsing history.
BASE_FEATURE(kMultistepFilter, base::FEATURE_DISABLED_BY_DEFAULT);

// The maximum number of `FilterAnnotation` candidates to process when
// generating suggestions.
BASE_FEATURE_PARAM(size_t,
                   kMultistepFilterSuggestionMaxCandidates,
                   &kMultistepFilter,
                   "suggestion_max_candidates",
                   10u);

BASE_FEATURE_PARAM(size_t,
                   kMultistepFilterMaxFacetsShownUkmClampingLimit,
                   &kMultistepFilter,
                   "max_facets_shown_ukm_clamping_limit",
                   10u);

// The duration for which filter annotations are considered valid.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kMultistepFilterSessionDuration,
                   &kMultistepFilter,
                   "filter_session_duration",
                   base::Minutes(30));

// The duration for which suggestions on the same domain are suppressed after
// extraction.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSameDomainSuggestionSuppressionDuration,
                   &kMultistepFilter,
                   base::Minutes(2));

// The duration after a suggestion is applied during which we track subsequent
// user actions (like going back or closing the tab) for metrics.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kMultistepFilterPostApplicationSessionDuration,
                   &kMultistepFilter,
                   base::Minutes(2));

// Enables the Send Feedback button in the contextual cue three-dot menu.
BASE_FEATURE(kMultistepFilterSendFeedback, base::FEATURE_DISABLED_BY_DEFAULT);

// The URL to navigate to when the Send Feedback button is clicked.
BASE_FEATURE_PARAM(std::string,
                   kMultistepFilterSendFeedbackUrl,
                   &kMultistepFilterSendFeedback,
                   "");

}  // namespace multistep_filter
