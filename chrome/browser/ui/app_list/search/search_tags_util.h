// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_TAGS_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_TAGS_UTIL_H_

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/omnibox/browser/autocomplete_match.h"

namespace app_list {

// Translates ACMatchClassifications into ChromeSearchResult tags.
void ACMatchClassificationsToTags(const std::u16string& text,
                                  const ACMatchClassifications& text_classes,
                                  ChromeSearchResult::Tags* tags);

// Calculates ChromeSearchResult tags for highlighting occurrences of |query| in
// |text|.
ChromeSearchResult::Tags CalculateTags(const std::u16string& query,
                                       const std::u16string& text);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_TAGS_UTIL_H_
