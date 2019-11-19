// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/ntp_snippets/features.h"

namespace ntp_snippets {

namespace {
// All categories must be present.
constexpr KnownCategories kKnownCategoriesDefaultOrder[] = {
    KnownCategories::READING_LIST,
    KnownCategories::ARTICLES,
};
}  // namespace

ConstantCategoryRanker::ConstantCategoryRanker() {
  std::vector<KnownCategories> ordered_known_categories =
      GetKnownCategoriesDefaultOrder();
  for (KnownCategories known_category : ordered_known_categories) {
    AppendKnownCategory(known_category);
  }
}

ConstantCategoryRanker::~ConstantCategoryRanker() = default;

bool ConstantCategoryRanker::Compare(Category left, Category right) const {
  if (!base::Contains(ordered_categories_, left)) {
    LOG(DFATAL) << "The category with ID " << left.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (!base::Contains(ordered_categories_, right)) {
    LOG(DFATAL) << "The category with ID " << right.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (left == right) {
    return false;
  }
  for (Category category : ordered_categories_) {
    if (category == left) {
      return true;
    }
    if (category == right) {
      return false;
    }
  }
  // This fallback is provided only to satisfy "Compare" contract if by mistake
  // categories are not added using AppendCategoryIfNecessary. One should not
  // rely on this, instead the order must be defined explicitly using
  // AppendCategoryIfNecessary.
  return left.id() < right.id();
}

void ConstantCategoryRanker::ClearHistory(base::Time begin, base::Time end) {
  // Ignored, because this implementation doesn't store any history-related
  // data.
}

void ConstantCategoryRanker::AppendCategoryIfNecessary(Category category) {
  if (!base::Contains(ordered_categories_, category)) {
    ordered_categories_.push_back(category);
  }
}

void ConstantCategoryRanker::InsertCategoryBeforeIfNecessary(
    Category category_to_insert,
    Category anchor) {
  // TODO(vitaliii): Implement.
  LOG(DFATAL) << "Not implemented, use ClickBasedCategoryRanker instead for "
                 "inserting categories relative to other categories.";
  AppendCategoryIfNecessary(category_to_insert);
}

void ConstantCategoryRanker::InsertCategoryAfterIfNecessary(
    Category category_to_insert,
    Category anchor) {
  // TODO(vitaliii): Implement.
  LOG(DFATAL) << "Not implemented, use ClickBasedCategoryRanker instead for "
                 "inserting categories relative to other categories.";
  AppendCategoryIfNecessary(category_to_insert);
}

std::vector<CategoryRanker::DebugDataItem>
ConstantCategoryRanker::GetDebugData() {
  std::vector<CategoryRanker::DebugDataItem> result;
  result.push_back(
      CategoryRanker::DebugDataItem("Type", "ConstantCategoryRanker"));

  std::vector<std::string> category_strings;
  for (Category category : ordered_categories_) {
    category_strings.push_back(base::NumberToString(category.id()));
  }
  result.push_back(CategoryRanker::DebugDataItem(
      "Current order", base::JoinString(category_strings, ", ")));

  return result;
}

void ConstantCategoryRanker::OnSuggestionOpened(Category category) {
  // Ignored. The order is constant.
}

void ConstantCategoryRanker::OnCategoryDismissed(Category category) {
  // Ignored. The order is constant.
}

// static
std::vector<KnownCategories>
ConstantCategoryRanker::GetKnownCategoriesDefaultOrder() {
  static_assert(
      static_cast<size_t>(KnownCategories::LOCAL_CATEGORIES_COUNT) == 6,
      "Number of local categories has changed, please update "
      "ConstantCategoryRanker::kKnownCategoriesDefaultOrder to list all "
      "local KnownCategories for all orders.");

  // Other remote categories will be ordered after these depending on when
  // providers notify us about them using AppendCategoryIfNecessary.
  return std::vector<KnownCategories>(kKnownCategoriesDefaultOrder,
                                      std::end(kKnownCategoriesDefaultOrder));
}

void ConstantCategoryRanker::AppendKnownCategory(
    KnownCategories known_category) {
  Category category = Category::FromKnownCategory(known_category);
  DCHECK(!base::Contains(ordered_categories_, category));
  ordered_categories_.push_back(category);
}

}  // namespace ntp_snippets
