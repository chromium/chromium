// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORING_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORING_H_

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"

namespace app_list {

// TODO(crbug.com/1258415): Consider refactoring so that |SortCategories| and
// |SortResults| are the lambda functions themselves, instead of thin wrappers
// around lambdas.

// Determine category display order by sorting the given list of |categories|
// in-place.
//
// Sorting criteria in descending precedence are:
// - Burn-in iteration number.
// - Score.
void SortCategories(CategoriesList& categories);

// Determine result display order by sorting the given list of |results|
// in-place. |categories| is assumed to be already sorted into category display
// order.
//
// Sorting criteria in descending precedence are:
// - Best match
// - Category
// - Burn-in iteration number
// - Display score.
void SortResults(std::vector<ChromeSearchResult*>& results,
                 const CategoriesList& categories);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_SCORING_H_
