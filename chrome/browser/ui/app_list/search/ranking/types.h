// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {

// The different categories of search result to display in launcher search.
// Every search result type maps to one category. These values are not stable,
// and should not be used for metrics.
enum class Category {
  kApp = 1,
  kWeb = 2,
  kFiles = 3,
  kAssistant = 4,
  kSettings = 5,
  kHelp = 6,
  kPlayStore = 7,
  kMaxValue = kPlayStore,
};

// All score information for a single result. This is stored with a result, and
// incrementally updated by rankers as needed. Generally, each ranker should
// control one score.
struct Scoring {
  bool filter = false;
  bool top_match = false;
  double normalized_relevance = 0.0f;
  double category_item_score = 0.0f;
  double category_usage_score = 0.0f;
  double usage_score = 0.0f;

  Scoring() {}

  Scoring(const Scoring&) = delete;
  Scoring& operator=(const Scoring&) = delete;

  double FinalScore() const;
};

::std::ostream& operator<<(::std::ostream& os, const Scoring& result);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_
