// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/fake_category_ranker.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"

namespace ntp_snippets {

FakeCategoryRanker::FakeCategoryRanker() = default;

FakeCategoryRanker::~FakeCategoryRanker() = default;

bool FakeCategoryRanker::Compare(Category left, Category right) const {
  DCHECK(base::Contains(categories_, left));
  DCHECK(base::Contains(categories_, right));

  return base::ranges::find(categories_, left) <
         base::ranges::find(categories_, right);
}

void FakeCategoryRanker::ClearHistory(base::Time begin, base::Time end) {
  // Ignored.
}

void FakeCategoryRanker::AppendCategoryIfNecessary(Category category) {
  // Ignored.
}

void FakeCategoryRanker::InsertCategoryBeforeIfNecessary(
    Category category_to_insert,
    Category anchor) {
  // Ignored.
}

void FakeCategoryRanker::InsertCategoryAfterIfNecessary(
    Category category_to_insert,
    Category anchor) {
  // Ignored.
}

std::vector<CategoryRanker::DebugDataItem> FakeCategoryRanker::GetDebugData() {
  // Ignored.
  return std::vector<CategoryRanker::DebugDataItem>();
}

void FakeCategoryRanker::OnSuggestionOpened(Category category) {
  // Ignored.
}

void FakeCategoryRanker::OnCategoryDismissed(Category category) {
  // Ignored.
}

}  // namespace ntp_snippets
