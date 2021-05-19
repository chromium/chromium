// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_RANKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_RANKER_H_

#include "chrome/browser/ui/app_list/search/ranking/ranker.h"

class Profile;

namespace app_list {
namespace {
enum class Category;
}  // namespace

class RecurrenceRanker;

// A ranker that groups results into categories.
class CategoryRanker : public Ranker {
 public:
  explicit CategoryRanker(Profile* profile);
  ~CategoryRanker() override;

  CategoryRanker(const CategoryRanker&) = delete;
  CategoryRanker& operator=(const CategoryRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query) override;
  void Rank(ResultsMap& results, ProviderType provider) override;
  void Train(const LaunchData& launch) override;

 private:
  void InitializeCategoryScores();

  std::unique_ptr<RecurrenceRanker> category_ranker_;

  base::flat_map<Category, int> category_ranks_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_CATEGORY_RANKER_H_
