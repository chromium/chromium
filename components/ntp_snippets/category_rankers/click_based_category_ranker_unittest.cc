// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/time_serialization.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace ntp_snippets {

namespace {

const char kHistogramMovedUpCategoryNewIndex[] =
    "NewTabPage.ContentSuggestions.MovedUpCategoryNewIndex";

}  // namespace

class ClickBasedCategoryRankerTest : public testing::Test {
 public:
  ClickBasedCategoryRankerTest()
      : pref_service_(std::make_unique<TestingPrefServiceSimple>()),
        unused_remote_category_id_(
            static_cast<int>(KnownCategories::LAST_KNOWN_REMOTE_CATEGORY) + 1) {
    ClickBasedCategoryRanker::RegisterProfilePrefs(pref_service_->registry());

    ranker_ = std::make_unique<ClickBasedCategoryRanker>(
        pref_service_.get(), base::DefaultClock::GetInstance());
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

  void ResetRanker(base::Clock* clock) {
    ranker_ =
        std::make_unique<ClickBasedCategoryRanker>(pref_service_.get(), clock);
  }

  void NotifyOnSuggestionOpened(int times, Category category) {
    for (int i = 0; i < times; ++i) {
      ranker()->OnSuggestionOpened(category);
    }
  }

  void NotifyOnCategoryDismissed(Category category) {
    ranker()->OnCategoryDismissed(category);
  }

  std::vector<Category> ConvertKnownCategories(
      std::vector<KnownCategories> known_categories) {
    std::vector<Category> converted;
    for (auto known : known_categories) {
      converted.push_back(Category::FromKnownCategory(known));
    }
    return converted;
  }

  ClickBasedCategoryRanker* ranker() { return ranker_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  int unused_remote_category_id_;
  std::unique_ptr<ClickBasedCategoryRanker> ranker_;

  DISALLOW_COPY_AND_ASSIGN(ClickBasedCategoryRankerTest);
};

TEST_F(ClickBasedCategoryRankerTest, ShouldSortRemoteCategoriesByWhenAdded) {
  const Category first = GetUnusedRemoteCategory();
  const Category second = GetUnusedRemoteCategory();
  // Categories are added in decreasing id order to test that they are not
  // compared by id.
  ranker()->AppendCategoryIfNecessary(second);
  ranker()->AppendCategoryIfNecessary(first);
  EXPECT_TRUE(CompareCategories(second, first));
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldSortLocalCategoriesBeforeRemote) {
  const Category remote_category = AddUnusedRemoteCategory();
  const Category local_category =
      Category::FromKnownCategory(KnownCategories::READING_LIST);
  EXPECT_TRUE(CompareCategories(local_category, remote_category));
  EXPECT_FALSE(CompareCategories(remote_category, local_category));
}

TEST_F(ClickBasedCategoryRankerTest,
       CompareShouldReturnFalseForSameCategories) {
  const Category remote_category = AddUnusedRemoteCategory();
  EXPECT_FALSE(CompareCategories(remote_category, remote_category));

  const Category local_category =
      Category::FromKnownCategory(KnownCategories::READING_LIST);
  EXPECT_FALSE(CompareCategories(local_category, local_category));
}

TEST_F(ClickBasedCategoryRankerTest,
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

TEST_F(ClickBasedCategoryRankerTest, ShouldChangeOrderOfNonTopCategories) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  ASSERT_FALSE(CompareCategories(second, first));

  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);

  EXPECT_TRUE(CompareCategories(second, first));
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotChangeOrderRightAfterOrderChange) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Two non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();
  ASSERT_TRUE(CompareCategories(first, second));
  ASSERT_FALSE(CompareCategories(second, first));
  // Their order is changed.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);
  ASSERT_TRUE(CompareCategories(second, first));
  ASSERT_FALSE(CompareCategories(first, second));

  // Click on the lower category.
  NotifyOnSuggestionOpened(/*times=*/1, first);

  // Order should not change.
  EXPECT_TRUE(CompareCategories(second, first));
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotMoveCategoryMoreThanOncePerClick) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();
  Category third = AddUnusedRemoteCategory();

  // Move the third category up.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), third);
  EXPECT_TRUE(CompareCategories(third, second));
  // But only on one position even though the first category has low counts.
  EXPECT_TRUE(CompareCategories(first, third));
  // However, another click must move it further.
  NotifyOnSuggestionOpened(/*times=*/1, third);
  EXPECT_TRUE(CompareCategories(third, first));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotMoveTopCategoryRightAfterThreshold) {
  ASSERT_GE(ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin(), 1);

  // At least one top category is added from the default order.
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  // Try to move the second category up as if the first category was non-top.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);

  // Nothing should change, because the first category is top.
  EXPECT_TRUE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldPersistOrderAndClicksWhenRestarted) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();
  Category third = AddUnusedRemoteCategory();

  // Change the order.
  NotifyOnSuggestionOpened(
      /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), third);
  ASSERT_TRUE(CompareCategories(third, second));
  ASSERT_TRUE(CompareCategories(first, third));

  // Simulate Chrome restart.
  ResetRanker(base::DefaultClock::GetInstance());

  // The old order must be preserved.
  EXPECT_TRUE(CompareCategories(third, second));

  // Clicks must be preserved as well.
  NotifyOnSuggestionOpened(/*times=*/1, third);
  EXPECT_TRUE(CompareCategories(third, first));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldDecayClickCountsWithTime) {
  // Add dummy remote categories to ensure that the following categories are not
  // in the top anymore.
  AddUnusedRemoteCategories(
      ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin());

  // Non-top categories are added.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  const int first_clicks = 10 * ClickBasedCategoryRanker::GetPassingMargin();

  // Simulate the user using the first category for a long time (and not using
  // anything else).
  NotifyOnSuggestionOpened(/*times=*/first_clicks, first);

  // Let multiple years pass by.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now() + base::TimeDelta::FromDays(1000));
  // Reset the ranker to pick up the new clock.
  ResetRanker(&test_clock);

  // The user behavior changes and they start using the second category instead.
  // According to our requirenments after such a long time it should take less
  // than |first_clicks| for the second category to outperform the first one.
  int second_clicks = 0;
  while (CompareCategories(first, second) && second_clicks < first_clicks) {
    NotifyOnSuggestionOpened(/*times=*/1, second);
    second_clicks++;
  }
  EXPECT_THAT(second_clicks, testing::Lt(first_clicks));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldDecayAfterClearHistory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  // The user clears entire history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // Check whether decay happens by clicking on the first category and
  // waiting.
  const int first_clicks = 10 * ClickBasedCategoryRanker::GetPassingMargin();
  NotifyOnSuggestionOpened(/*times=*/first_clicks, first);

  // Let multiple years pass by.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now() + base::TimeDelta::FromDays(1000));
  // Reset the ranker to pick up the new clock.
  ResetRanker(&test_clock);

  // It should take less than |first_clicks| for the second category to
  // overtake because of decays.
  int second_clicks = 0;
  while (CompareCategories(first, second) && second_clicks < first_clicks) {
    NotifyOnSuggestionOpened(/*times=*/1, second);
    second_clicks++;
  }
  EXPECT_THAT(second_clicks, testing::Lt(first_clicks));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldRemoveLastDecayTimeOnClearHistory) {
  ASSERT_NE(ranker()->GetLastDecayTime(), DeserializeTime(0));

  // The user clears entire history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  EXPECT_EQ(ranker()->GetLastDecayTime(), DeserializeTime(0));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldPersistLastDecayTimeWhenRestarted) {
  base::Time before = ranker()->GetLastDecayTime();
  ASSERT_NE(before, DeserializeTime(0));

  // Ensure that |Now()| is different from |before| by injecting our clock.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now() + base::TimeDelta::FromSeconds(10));
  ResetRanker(&test_clock);

  EXPECT_EQ(before, ranker()->GetLastDecayTime());
}

TEST_F(ClickBasedCategoryRankerTest, ShouldMoveCategoryDownWhenDismissed) {
  // Take top categories.
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));
  NotifyOnCategoryDismissed(first);
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldMoveSecondToLastCategoryDownWhenDismissed) {
  // Add categories to the bottom.
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  NotifyOnCategoryDismissed(first);
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotMoveCategoryTooMuchDownWhenDismissed) {
  // Add enough categories to the end.
  std::vector<Category> categories;
  const int penalty = ClickBasedCategoryRanker::GetDismissedCategoryPenalty();
  for (int i = 0; i < 2 * penalty + 10; ++i) {
    categories.push_back(AddUnusedRemoteCategory());
  }

  const int target = penalty + 1;
  Category target_category = categories[target];
  for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
    ASSERT_EQ(i < target, CompareCategories(categories[i], target_category));
  }

  // This should move exactly |penalty| categories up.
  NotifyOnCategoryDismissed(categories[target]);

  // Reflect expected change in |categories|.
  const int expected = target + penalty;
  for (int i = target; i + 1 <= expected; ++i) {
    std::swap(categories[i], categories[i + 1]);
  }

  for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
    EXPECT_EQ(i < expected, CompareCategories(categories[i], target_category));
  }
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotChangeOrderOfOtherCategoriesWhenDismissed) {
  // Add enough categories to the end.
  std::vector<Category> categories;
  const int penalty = ClickBasedCategoryRanker::GetDismissedCategoryPenalty();
  for (int i = 0; i < 2 * penalty + 10; ++i) {
    categories.push_back(AddUnusedRemoteCategory());
  }

  int target = penalty + 1;
  // This should not change order of all other categories.
  NotifyOnCategoryDismissed(categories[target]);

  categories.erase(categories.begin() + target);
  for (int first = 0; first < static_cast<int>(categories.size()); ++first) {
    for (int second = 0; second < static_cast<int>(categories.size());
         ++second) {
      EXPECT_EQ(first < second,
                CompareCategories(categories[first], categories[second]));
    }
  }
}

TEST_F(ClickBasedCategoryRankerTest, ShouldNotMoveLastCategoryWhenDismissed) {
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));
  NotifyOnCategoryDismissed(second);
  EXPECT_TRUE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldRestoreDefaultOrderOnClearHistory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  // Change the order.
  while (CompareCategories(first, second)) {
    NotifyOnSuggestionOpened(
        /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);
  }

  ASSERT_FALSE(CompareCategories(first, second));

  // The user clears history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // The default order must be restored.
  EXPECT_TRUE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldPreserveRemoteCategoriesOnClearHistory) {
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));

  // The user clears history.
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // The order does not matter, but the ranker should not die.
  CompareCategories(first, second);
}

TEST_F(ClickBasedCategoryRankerTest, ShouldIgnorePartialClearHistory) {
  Category first = AddUnusedRemoteCategory();
  Category second = AddUnusedRemoteCategory();

  ASSERT_TRUE(CompareCategories(first, second));

  // Change the order.
  while (CompareCategories(first, second)) {
    NotifyOnSuggestionOpened(
        /*times=*/ClickBasedCategoryRanker::GetPassingMargin(), second);
  }

  ASSERT_FALSE(CompareCategories(first, second));

  // The user partially clears history.
  base::Time begin = base::Time::Now() - base::TimeDelta::FromHours(1),
             end = base::Time::Max();
  ranker()->ClearHistory(begin, end);

  // The order should not be cleared.
  EXPECT_FALSE(CompareCategories(first, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldEmitNewIndexWhenCategoryMovedUpDueToClick) {
  base::HistogramTester histogram_tester;

  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  // Increase the score of |second| until the order changes.
  while (CompareCategories(first, second)) {
    EXPECT_THAT(
        histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
        IsEmpty());
    ranker()->OnSuggestionOpened(second);
  }
  ASSERT_FALSE(CompareCategories(first, second));
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotEmitNewIndexWhenCategoryDismissed) {
  base::HistogramTester histogram_tester;

  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category category = Category::FromKnownCategory(default_order[0]);

  ASSERT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());

  NotifyOnCategoryDismissed(category);

  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotEmitNewIndexOfMovedUpCategoryWhenHistoryCleared) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  // Increase the score of |second| until the order changes.
  while (CompareCategories(first, second)) {
    ranker()->OnSuggestionOpened(second);
  }
  ASSERT_FALSE(CompareCategories(first, second));

  // The histogram tester is created here to ignore previous events.
  base::HistogramTester histogram_tester;
  ranker()->ClearHistory(/*begin=*/base::Time(),
                         /*end=*/base::Time::Max());

  // ClearHistory should restore the default order.
  ASSERT_TRUE(CompareCategories(first, second));

  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldInsertCategoryBeforeSelectedCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  Category inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryBeforeIfNecessary(inserted, second);
  EXPECT_TRUE(CompareCategories(first, inserted));
  EXPECT_TRUE(CompareCategories(inserted, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldInsertMultipleCategoriesBeforeSelectedCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  Category first_inserted = GetUnusedRemoteCategory();
  Category second_inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryBeforeIfNecessary(first_inserted, second);
  ranker()->InsertCategoryBeforeIfNecessary(second_inserted, second);
  EXPECT_TRUE(CompareCategories(first, first_inserted));
  EXPECT_TRUE(CompareCategories(first_inserted, second_inserted));
  EXPECT_TRUE(CompareCategories(second_inserted, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldInsertCategoryBeforeFirstCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryBeforeIfNecessary(inserted, first);
  EXPECT_TRUE(CompareCategories(inserted, first));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldInsertCategoryBeforeRemoteCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category remote = AddUnusedRemoteCategory();
  Category inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryBeforeIfNecessary(inserted, remote);
  EXPECT_TRUE(CompareCategories(inserted, remote));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotChangeRemainingOrderWhenInsertingBeforeCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category anchor = Category::FromKnownCategory(default_order[0]);
  Category inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryBeforeIfNecessary(inserted, anchor);
  std::vector<Category> converted_categories =
      ConvertKnownCategories(default_order);
  for (size_t i = 0; i + 1 < converted_categories.size(); ++i) {
    EXPECT_TRUE(CompareCategories(converted_categories[i],
                                  converted_categories[i + 1]));
  }
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldInsertCategoriesBeforeAndAfterSameCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);
  ASSERT_TRUE(CompareCategories(first, second));

  Category first_before = GetUnusedRemoteCategory();
  ranker()->InsertCategoryBeforeIfNecessary(first_before, second);

  Category first_after = GetUnusedRemoteCategory();
  ranker()->InsertCategoryAfterIfNecessary(first_after, second);

  Category second_before = GetUnusedRemoteCategory();
  ranker()->InsertCategoryBeforeIfNecessary(second_before, second);

  Category second_after = GetUnusedRemoteCategory();
  ranker()->InsertCategoryAfterIfNecessary(second_after, second);

  EXPECT_TRUE(CompareCategories(first_before, second_before));
  EXPECT_TRUE(CompareCategories(second_before, second));
  EXPECT_TRUE(CompareCategories(second, second_after));
  EXPECT_TRUE(CompareCategories(second_after, first_after));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldInsertCategoriesBeforeAndAfterDifferentCategories) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);
  ASSERT_TRUE(CompareCategories(first, second));

  Category first_before = GetUnusedRemoteCategory();
  ranker()->InsertCategoryBeforeIfNecessary(first_before, second);

  Category first_after = GetUnusedRemoteCategory();
  ranker()->InsertCategoryAfterIfNecessary(first_after, first);

  Category second_before = GetUnusedRemoteCategory();
  ranker()->InsertCategoryBeforeIfNecessary(second_before, second);

  Category second_after = GetUnusedRemoteCategory();
  ranker()->InsertCategoryAfterIfNecessary(second_after, first);

  EXPECT_TRUE(CompareCategories(first, second_after));
  EXPECT_TRUE(CompareCategories(second_after, first_after));
  EXPECT_TRUE(CompareCategories(first_after, first_before));
  EXPECT_TRUE(CompareCategories(first_before, second_before));
  EXPECT_TRUE(CompareCategories(second_before, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotEmitNewIndexWhenCategoryInserted) {
  base::HistogramTester histogram_tester;

  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);

  ASSERT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());

  Category before = GetUnusedRemoteCategory();
  ranker()->InsertCategoryBeforeIfNecessary(before, first);

  Category after = GetUnusedRemoteCategory();
  ranker()->InsertCategoryAfterIfNecessary(after, first);

  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramMovedUpCategoryNewIndex),
              IsEmpty());
}

// TODO(vitaliii): Reuse these tests for ConstantCategoryRanker.
TEST_F(ClickBasedCategoryRankerTest,
       ShouldInsertCategoryAfterSelectedCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  Category inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryAfterIfNecessary(inserted, first);
  EXPECT_TRUE(CompareCategories(first, inserted));
  EXPECT_TRUE(CompareCategories(inserted, second));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldInsertMultipleCategoriesAfterSelectedCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category first = Category::FromKnownCategory(default_order[0]);
  Category second = Category::FromKnownCategory(default_order[1]);

  ASSERT_TRUE(CompareCategories(first, second));

  Category first_inserted = GetUnusedRemoteCategory();
  Category second_inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryAfterIfNecessary(first_inserted, first);
  ranker()->InsertCategoryAfterIfNecessary(second_inserted, first);
  EXPECT_TRUE(CompareCategories(first, second_inserted));
  EXPECT_TRUE(CompareCategories(second_inserted, first_inserted));
  EXPECT_TRUE(CompareCategories(first_inserted, second));
}

TEST_F(ClickBasedCategoryRankerTest, ShouldInsertCategoryAfterLastCategory) {
  Category last = AddUnusedRemoteCategory();
  Category inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryAfterIfNecessary(inserted, last);
  EXPECT_TRUE(CompareCategories(last, inserted));
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldNotChangeRemainingOrderWhenInsertingAfterCategory) {
  std::vector<KnownCategories> default_order =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();
  Category anchor = Category::FromKnownCategory(default_order[0]);
  Category inserted = GetUnusedRemoteCategory();

  ranker()->InsertCategoryAfterIfNecessary(inserted, anchor);
  std::vector<Category> converted_categories =
      ConvertKnownCategories(default_order);
  for (size_t i = 0; i + 1 < converted_categories.size(); ++i) {
    EXPECT_TRUE(CompareCategories(converted_categories[i],
                                  converted_categories[i + 1]));
  }
}

TEST_F(ClickBasedCategoryRankerTest,
       ShouldAssignScoreToInsertedCategoriesBasedOnAnchor) {
  Category anchor = AddUnusedRemoteCategory();
  NotifyOnSuggestionOpened(/*times=*/25, anchor);

  Category inserted_before = GetUnusedRemoteCategory();
  ranker()->InsertCategoryBeforeIfNecessary(inserted_before, anchor);

  Category inserted_after = GetUnusedRemoteCategory();
  ranker()->InsertCategoryAfterIfNecessary(inserted_after, anchor);

  Category tester = AddUnusedRemoteCategory();
  NotifyOnSuggestionOpened(/*times=*/20, tester);
  EXPECT_TRUE(CompareCategories(inserted_before, tester));
  EXPECT_TRUE(CompareCategories(inserted_after, tester));
}

}  // namespace ntp_snippets
