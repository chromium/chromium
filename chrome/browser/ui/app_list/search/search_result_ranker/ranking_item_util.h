// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RANKING_ITEM_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RANKING_ITEM_UTIL_H_

#include <string>

class ChromeAppListItem;
class ChromeSearchResult;

namespace app_list {

// A simplified value describing what kind a search result should be treated as
// for the purposes of ranking. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused.
enum class RankingItemType {
  kUnknown = 0,
  kIgnored = 1,
  kFile = 2,
  kApp = 3,
  kOmniboxGeneric = 4,
  kArcAppShortcut = 5,
  kZeroStateFile = 6,
  kDriveQuickAccess = 7,
  // Deprecated:
  // kChip = 8,
  kZeroStateFileChip = 9,
  kDriveQuickAccessChip = 10,
  // Add new types above this line.
  kMaxValue = kDriveQuickAccessChip,
};

// Convert a |ChromeSearchResult| into its |RankingItemType|.
RankingItemType RankingItemTypeFromSearchResult(
    const ChromeSearchResult& result);

// Return the type of an |ChromeAppListItem|. We currently do not distinguish
// between different kinds of apps, and all |AppServiceAppItem|s are apps, so we
// trivially return |kApp|.
RankingItemType RankingItemTypeFromChromeAppListItem(
    const ChromeAppListItem& item);

// Normalizes app IDs by removing any scheme prefix and trailing slash:
// "arc://[id]/" to "[id]". This is necessary because apps launched from
// different parts of the launcher have differently formatted IDs.
std::string NormalizeAppId(const std::string& id);

// Given a search result ID representing a URL, removes some components of the
// URL such as the query and fragment. This is intended to normalize URLs that
// should be considered the same for the purposes of ranking.
std::string SimplifyUrlId(const std::string& url_id);

// Given a search result ID representing a google docs file, remove parts of the
// URL that can vary without affecting what doc the URL resolves to.
std::string SimplifyGoogleDocsUrlId(const std::string& url_id);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RANKING_ITEM_UTIL_H_
