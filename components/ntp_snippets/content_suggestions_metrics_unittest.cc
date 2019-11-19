// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {
namespace metrics {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;

TEST(ContentSuggestionsMetricsTest, ShouldLogOnSuggestionsShown) {
  base::HistogramTester histogram_tester;
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*position_in_category=*/3, base::Time::Now(),
                    /*score=*/0.01f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  // Test corner cases for score.
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*position_in_category=*/3, base::Time::Now(),
                    /*score=*/0.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*position_in_category=*/3, base::Time::Now(),
                    /*score=*/1.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));
  OnSuggestionShown(/*global_position=*/1,
                    Category::FromKnownCategory(KnownCategories::ARTICLES),
                    /*position_in_category=*/3, base::Time::Now(),
                    /*score=*/8.0f,
                    base::Time::Now() - base::TimeDelta::FromHours(2));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.ShownScoreNormalized.Articles"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                  base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/10, /*count=*/1),
                  base::Bucket(/*min=*/11, /*count=*/1)));
}

TEST(ContentSuggestionsMetricsTest,
     ShouldNotLogNotShownCategoriesWhenPageShown) {
  base::HistogramTester histogram_tester;
  OnPageShown(std::vector<Category>(
                  {Category::FromKnownCategory(KnownCategories::ARTICLES)}),
              /*suggestions_per_category=*/{0},
              /*is_category_visible=*/{false});
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible.Articles"),
      IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.SectionCountOnNtpOpened"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
}

TEST(ContentSuggestionsMetricsTest,
     ShouldLogEmptyShownCategoriesWhenPageShown) {
  base::HistogramTester histogram_tester;
  OnPageShown(std::vector<Category>(
                  {Category::FromKnownCategory(KnownCategories::ARTICLES)}),
              /*suggestions_per_category=*/{0},
              /*is_category_visible=*/{true});
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible.Articles"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.SectionCountOnNtpOpened"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
}

TEST(ContentSuggestionsMetricsTest,
     ShouldLogNonEmptyShownCategoryWhenPageShown) {
  base::HistogramTester histogram_tester;
  OnPageShown(std::vector<Category>(
                  {Category::FromKnownCategory(KnownCategories::ARTICLES)}),
              /*suggestions_per_category=*/{10},
              /*is_category_visible=*/{true});
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible.Articles"),
      ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.SectionCountOnNtpOpened"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
}

TEST(ContentSuggestionsMetricsTest,
     ShouldLogMultipleNonEmptyShownCategoriesWhenPageShown) {
  base::HistogramTester histogram_tester;
  OnPageShown(std::vector<Category>(
                  {Category::FromKnownCategory(KnownCategories::ARTICLES),
                   Category::FromKnownCategory(KnownCategories::READING_LIST)}),
              /*suggestions_per_category=*/{10, 5},
              /*is_category_visible=*/{true, true});
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible.Articles"),
      ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible"),
              ElementsAre(base::Bucket(/*min=*/15, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.SectionCountOnNtpOpened"),
              ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
}

TEST(ContentSuggestionsMetricsTest, ShouldLogPrefetchedSuggestionsWhenOpened) {
  base::HistogramTester histogram_tester;
  OnSuggestionOpened(/*global_position=*/11,
                     Category::FromKnownCategory(KnownCategories::ARTICLES),
                     /*category_index=*/2,
                     /*position_in_category=*/1, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::NEW_BACKGROUND_TAB,
                     /*is_prefetched=*/false, /*is_offline=*/false);
  OnSuggestionOpened(/*global_position=*/13,
                     Category::FromKnownCategory(KnownCategories::ARTICLES),
                     /*category_index=*/2,
                     /*position_in_category=*/3, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::NEW_BACKGROUND_TAB,
                     /*is_prefetched=*/true, /*is_offline=*/false);
  OnSuggestionOpened(/*global_position=*/15,
                     Category::FromKnownCategory(KnownCategories::ARTICLES),
                     /*category_index=*/2,
                     /*position_in_category=*/5, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::NEW_BACKGROUND_TAB,
                     /*is_prefetched=*/false, /*is_offline=*/true);
  OnSuggestionOpened(/*global_position=*/23,
                     Category::FromKnownCategory(KnownCategories::ARTICLES),
                     /*category_index=*/2,
                     /*position_in_category=*/13, base::Time::Now(),
                     /*score=*/1.0f, WindowOpenDisposition::NEW_BACKGROUND_TAB,
                     /*is_prefetched=*/true, /*is_offline=*/true);

  // Only the last call should be reported, because only there the user is
  // offline and the suggestion was prefetched at the same time.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.Opened.Articles.Prefetched.Offline"),
      ElementsAre(base::Bucket(/*min=*/13, /*count=*/1)));
}

}  // namespace
}  // namespace metrics
}  // namespace ntp_snippets
