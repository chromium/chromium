// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMPANION_VISUAL_SEARCH_FEATURES_H_
#define CHROME_COMMON_COMPANION_VISUAL_SEARCH_FEATURES_H_

#include "base/feature_list.h"

namespace companion::visual_search::features {

// Enables visual search capabilities for the companion.
BASE_DECLARE_FEATURE(kVisualSearchSuggestions);

// Enables triggering visual search capabilities from renderer agent.
// This flag is mainly used to test the model download mechanism and guard
// against breakages from pushing new models to production.
BASE_DECLARE_FEATURE(kVisualSearchSuggestionsAgent);

// Determines if visual suggestions is enabled.
bool IsVisualSearchSuggestionsEnabled();

// Determines if visual suggestions by agent is enabled.
bool IsVisualSearchSuggestionsAgentEnabled();

}  // namespace companion::visual_search::features
#endif  // CHROME_COMMON_COMPANION_VISUAL_SEARCH_FEATURES_H_
