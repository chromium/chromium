// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/types.h"

#include <stddef.h>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace app_list {

double Scoring::FinalScore() const {
  // Don't make any calls for Finch parameters in this method. If needed, put
  // them in an anonymous namespace above.
  if (filter)
    return -1.0;

  return normalized_relevance + category_item_score * 10.0 +
         category_usage_score * 10.0 + usage_score * 10.0 +
         (top_match ? 1000.0 : 0.0);
}

::std::ostream& operator<<(::std::ostream& os, const Scoring& scoring) {
  if (scoring.filter)
    return os << "{" << scoring.FinalScore() << " | filtered}";
  return os << base::StringPrintf(
             "{%.2f | nr:%.2f ci:%.2f cu:%.2f u:%.2f tm:%d}",
             scoring.FinalScore(), scoring.normalized_relevance,
             scoring.category_item_score, scoring.category_usage_score,
             scoring.usage_score, scoring.top_match);
}

CategoriesList CreateAllCategories() {
  CategoriesList res(
      {{.category = Category::kApps, .score = 0.0},
       {.category = Category::kAppShortcuts, .score = 0.0},
       {.category = Category::kWeb, .score = 0.0},
       {.category = Category::kFiles, .score = 0.0},
       {.category = Category::kSettings, .score = 0.0},
       {.category = Category::kHelp, .score = 0.0},
       {.category = Category::kPlayStore, .score = 0.0},
       {.category = Category::kSearchAndAssistant, .score = 0.0}});
  DCHECK_EQ(res.size(), static_cast<size_t>(Category::kMaxValue));
  return res;
}

}  // namespace app_list
