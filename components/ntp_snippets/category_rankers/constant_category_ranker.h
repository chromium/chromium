// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CONSTANT_CATEGORY_RANKER_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CONSTANT_CATEGORY_RANKER_H_

#include <vector>

#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"

namespace ntp_snippets {

// Simple implementation of a CategoryRanker that never changes the order. For
// KnownCategories, the order is hardcoded. Remote categories must be added
// using |AppendCategoryIfNecessary|. They are sorted after all known
// categories. Among themselves their order is the same as the order they were
// added in.
class ConstantCategoryRanker : public CategoryRanker {
 public:
  ConstantCategoryRanker();
  ConstantCategoryRanker(const ConstantCategoryRanker&) = delete;
  ConstantCategoryRanker& operator=(const ConstantCategoryRanker&) = delete;
  ~ConstantCategoryRanker() override;

  // CategoryRanker implementation.
  bool Compare(Category left, Category right) const override;
  void ClearHistory(base::Time begin, base::Time end) override;
  void AppendCategoryIfNecessary(Category category) override;
  void InsertCategoryBeforeIfNecessary(Category category_to_insert,
                                       Category anchor) override;
  void InsertCategoryAfterIfNecessary(Category category_to_insert,
                                      Category anchor) override;
  std::vector<CategoryRanker::DebugDataItem> GetDebugData() override;
  void OnSuggestionOpened(Category category) override;
  void OnCategoryDismissed(Category category) override;

  static std::vector<KnownCategories> GetKnownCategoriesDefaultOrder();

 private:
  void AppendKnownCategory(KnownCategories known_category);

  std::vector<Category> ordered_categories_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_CONSTANT_CATEGORY_RANKER_H_
