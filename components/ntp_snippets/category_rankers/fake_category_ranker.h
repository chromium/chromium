// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_FAKE_CATEGORY_RANKER_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_FAKE_CATEGORY_RANKER_H_

#include <vector>

#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ntp_snippets {

class FakeCategoryRanker : public CategoryRanker {
 public:
  FakeCategoryRanker();
  ~FakeCategoryRanker() override;

  void SetOrder(const std::vector<Category>& new_order) {
    categories_ = new_order;
  }

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

 private:
  std::vector<Category> categories_;
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_FAKE_CATEGORY_RANKER_H_
