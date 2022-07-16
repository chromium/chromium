// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CONSTANTS_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CONSTANTS_H_

namespace app_list {

// The maximum number of omnibox results to display if we have more results than
// can fit in the UI.
constexpr int kMaxOmniboxResults = 3;

// The maximum number of top matches to show.
constexpr size_t kNumTopMatches = 3u;

// The score threshold before we consider a result a top match.
constexpr double kTopMatchThreshold = 0.9;

// String to add to the details text of top match results. Keep the char[] and
// char16_t versions in sync.
// TODO(crbug.com/1199206): Once the UI has support for categories these can be
// removed.
constexpr char kTopMatchDetails[] = "(top match) ";
constexpr char16_t kTopMatchDetailsUTF16[] = u"(top match) ";

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CONSTANTS_H_
