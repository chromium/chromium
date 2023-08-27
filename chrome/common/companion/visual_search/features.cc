// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/companion/visual_search/features.h"

#include "base/feature_list.h"

namespace companion::visual_search::features {

BASE_FEATURE(kVisualSearchSuggestions,
             "VisualSearchSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVisualSearchSuggestionsAgent,
             "VisualSearchSuggestionsAgent",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsVisualSearchSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kVisualSearchSuggestions);
}

bool IsVisualSearchSuggestionsAgentEnabled() {
  return base::FeatureList::IsEnabled(kVisualSearchSuggestionsAgent);
}

}  // namespace companion::visual_search::features
