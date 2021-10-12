// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_TYPES_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_path.h"

namespace app_list {

using Category = ash::AppListSearchResultCategory;

// All score information for a single result. This is stored with a result, and
// incrementally updated by rankers as needed. Generally, each ranker should
// control one score.
//
// TODO(crbug.com/1199206): Category scores need to be removed from this and
// added to a separate struct.
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
