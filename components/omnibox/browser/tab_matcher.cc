// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/tab_matcher.h"

void TabMatcher::FindMatchingTabs(GURLToTabInfoMap* map,
                                  const AutocompleteInput* input) const {
  for (auto& gurl_to_tab_info : *map) {
    gurl_to_tab_info.second.has_matching_tab =
        IsTabOpenWithURL(gurl_to_tab_info.first, input);
  }
}

bool TabMatcher::IsTabOpenWithSameTitleOrSimilarURL(
    const std::u16string& title,
    const GURL& url,
    const GURL::Replacements& replacements,
    bool exclude_active_tab) const {
  return false;
}

std::vector<TabMatcher::TabWrapper> TabMatcher::GetOpenTabs(
    const AutocompleteInput* input,
    bool exclude_active_tab) const {
  return std::vector<TabMatcher::TabWrapper>();
}
