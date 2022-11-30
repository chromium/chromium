// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_TAGS_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_TAGS_UTIL_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/autocomplete_match.h"

namespace app_list {

// Translates ACMatchClassifications into ChromeSearchResult tags.
void ACMatchClassificationsToTags(const std::u16string& text,
                                  const ACMatchClassifications& text_classes,
                                  ChromeSearchResult::Tags* tags,
                                  const bool ignore_match = false);

// TODO(crbug.com/1258415): Remove this and its references once the productivity
// launcher is enabled.
ChromeSearchResult::Tags CalculateTags(const std::u16string& query,
                                       const std::u16string& text);

// Calculates ChromeSearchResult tags for highlighting occurrences of |query| in
// |text|, and appends them to supplied Tags.
void AppendMatchTags(const std::u16string& query,
                     const std::u16string& text,
                     ChromeSearchResult::Tags* tags);

// Returns the tag vector for the given text type.
ash::SearchResultTags TagsForText(const std::u16string& text,
                                  crosapi::mojom::SearchResult::TextType type);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_TAGS_UTIL_H_
