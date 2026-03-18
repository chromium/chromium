// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/features.h"

#include <cstddef>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

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

// A comma-separated list of domains that are allowed to trigger the Multistep
// Filter feature. If the value is "*", all domains are allowed.
BASE_FEATURE_PARAM(std::string,
                   kMultistepFilterAllowedDomains,
                   &kMultistepFilter,
                   "allowed_domains",
                   "*");

}  // namespace multistep_filter
