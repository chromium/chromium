// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_FEATURES_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_FEATURES_H_

#include <cstddef>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace multistep_filter {

BASE_DECLARE_FEATURE(kMultistepFilter);
BASE_DECLARE_FEATURE_PARAM(size_t, kMultistepFilterSuggestionMaxCandidates);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_FEATURES_H_
