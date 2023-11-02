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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::vector<content::WebContents*> TabMatcher::GetOpenTabs() const {
  return std::vector<content::WebContents*>();
}
#endif
