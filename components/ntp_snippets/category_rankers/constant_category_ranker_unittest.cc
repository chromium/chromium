// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"

#include "components/ntp_snippets/category.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class ConstantCategoryRankerTest : public testing::Test {
 public:
  ConstantCategoryRankerTest()
      : unused_remote_category_id_(
            static_cast<int>(KnownCategories::LAST_KNOWN_REMOTE_CATEGORY) + 1) {
  }

  int GetUnusedRemoteCategoryID() { return unused_remote_category_id_++; }

  Category GetUnusedRemoteCategory() {
    return Category::FromIDValue(GetUnusedRemoteCategoryID());
  }

  bool CompareCategories(const Category& left, const Category& right) {
    return ranker()->Compare(left, right);
  }

  Category AddUnusedRemoteCategory() {
    Category category = GetUnusedRemoteCategory();
    ranker()->AppendCategoryIfNecessary(category);
    return category;
  }

  void AddUnusedRemoteCategories(int quantity) {
    for (int i = 0; i < quantity; ++i) {
      AddUnusedRemoteCategory();
    }
  }

  ConstantCategoryRanker* ranker() { return &ranker_; }

 private:
  ConstantCategoryRanker ranker_;
  int unused_remote_category_id_;

  DISALLOW_COPY_AND_ASSIGN(ConstantCategoryRankerTest);
};

TEST_F(ConstantCategoryRankerTest, ShouldSortRemoteCategoriesByWhenAdded) {
  const Category first = GetUnusedRemoteCategory();
  const Category second = GetUnusedRemoteCategory();
  // Categories are added in decreasing id order to test that they are not
  // compared by id.
  ranker()->AppendCategoryIfNecessary(second);
  ranker()->AppendCategoryIfNecessary(first);
  EXPECT_TRUE(CompareCategories(second, first));
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ConstantCategoryRankerTest, ShouldSortLocalCategoriesBeforeRemote) {
  const Category remote_category = AddUnusedRemoteCategory();
  const Category local_category =
      Category::FromKnownCategory(KnownCategories::READING_LIST);
  EXPECT_TRUE(CompareCategories(local_category, remote_category));
  EXPECT_FALSE(CompareCategories(remote_category, local_category));
}

TEST_F(ConstantCategoryRankerTest, CompareShouldReturnFalseForSameCategories) {
  const Category remote_category = AddUnusedRemoteCategory();
  EXPECT_FALSE(CompareCategories(remote_category, remote_category));

  const Category local_category =
      Category::FromKnownCategory(KnownCategories::READING_LIST);
  EXPECT_FALSE(CompareCategories(local_category, local_category));
}

TEST_F(ConstantCategoryRankerTest,
       AddingMoreRemoteCategoriesShouldNotChangePreviousOrder) {
  AddUnusedRemoteCategories(3);

  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  ASSERT_FALSE(CompareCategories(second, first));

  AddUnusedRemoteCategories(3);

  EXPECT_TRUE(CompareCategories(first, second));
  EXPECT_FALSE(CompareCategories(second, first));
}

TEST_F(ConstantCategoryRankerTest,
       AddingSameCategoryTwiceShouldNotChangeOrder) {
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  ASSERT_FALSE(CompareCategories(second, first));

  ranker()->AppendCategoryIfNecessary(first);

  EXPECT_TRUE(CompareCategories(first, second));
  EXPECT_FALSE(CompareCategories(second, first));
}

TEST_F(ConstantCategoryRankerTest, ShouldSortNonConsequtiveRemoteCategories) {
  AddUnusedRemoteCategories(3);

  Category first = AddUnusedRemoteCategory();

  AddUnusedRemoteCategories(3);

  Category second = AddUnusedRemoteCategory();

  AddUnusedRemoteCategories(3);

  EXPECT_TRUE(CompareCategories(first, second));
  EXPECT_FALSE(CompareCategories(second, first));
}

}  // namespace ntp_snippets
