// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_MOCK_CATEGORY_RANKER_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_MOCK_CATEGORY_RANKER_H_

#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ntp_snippets {

class MockCategoryRanker : public CategoryRanker {
 public:
  MockCategoryRanker();
  ~MockCategoryRanker() override;

  MOCK_CONST_METHOD2(Compare, bool(Category left, Category right));
  MOCK_METHOD2(ClearHistory, void(base::Time begin, base::Time end));
  MOCK_METHOD1(AppendCategoryIfNecessary, void(Category category));
  MOCK_METHOD2(InsertCategoryBeforeIfNecessary,
               void(Category category_to_insert, Category anchor));
  MOCK_METHOD2(InsertCategoryAfterIfNecessary,
               void(Category category_to_insert, Category anchor));
  MOCK_METHOD0(GetDebugData, std::vector<CategoryRanker::DebugDataItem>());
  MOCK_METHOD1(OnSuggestionOpened, void(Category category));
  MOCK_METHOD1(OnCategoryDismissed, void(Category Category));
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_RANKERS_MOCK_CATEGORY_RANKER_H_
