// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_row.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace history {

namespace {

TEST(HistoryUrlRowTest, MergeCategoryIntoVector) {
  std::vector<VisitContentModelAnnotations::Category> categories;
  categories.emplace_back("category1", 40);
  categories.emplace_back("category2", 20);

  VisitContentModelAnnotations::MergeCategoryIntoVector({"category1", 50},
                                                        &categories);
  EXPECT_THAT(
      categories,
      ElementsAre(VisitContentModelAnnotations::Category("category1", 50),
                  VisitContentModelAnnotations::Category("category2", 20)));

  VisitContentModelAnnotations::MergeCategoryIntoVector({"category3", 30},
                                                        &categories);
  EXPECT_THAT(
      categories,
      ElementsAre(VisitContentModelAnnotations::Category("category1", 50),
                  VisitContentModelAnnotations::Category("category2", 20),
                  VisitContentModelAnnotations::Category("category3", 30)));
}

TEST(HistoryUrlRowTest, MergeVisibilityScores) {
  struct TestCase {
    std::string label;
    double starting_score;
    double merge_score;
    double want_score;
  };
  struct TestCase tests[] = {
      {
          // Regression test for http://crbug.com/1411063
          .label = "Default score is overwritten",
          .starting_score = -1,
          .merge_score = 0.5,
          .want_score = 0.5,
      },
      {
          .label = "Default score is overwritten with 0",
          .starting_score = -1,
          .merge_score = 0,
          .want_score = 0,
      },
      {
          .label = "Set score is not overwritten",
          .starting_score = 0.5,
          .merge_score = -1,
          .want_score = 0.5,
      },
      {
          .label = "Set score of 0 is not overwritten",
          .starting_score = 0,
          .merge_score = -1,
          .want_score = 0,
      },
      {
          .label = "Takes the lowest of two set scores, merge is lower",
          .starting_score = 0.5,
          .merge_score = 0,
          .want_score = 0,
      },
      {
          .label = "Takes the lowest of two set scores, starting is lower",
          .starting_score = 0,
          .merge_score = 0.5,
          .want_score = 0,
      },
      {
          .label = "Both default values is ignored",
          .starting_score = -1,
          .merge_score = -1,
          .want_score = -1,
      },
  };

  for (const TestCase& test : tests) {
    SCOPED_TRACE(test.label);
    VisitContentModelAnnotations starting;
    starting.visibility_score = test.starting_score;

    VisitContentModelAnnotations merge_me;
    merge_me.visibility_score = test.merge_score;

    starting.MergeFrom(merge_me);

    EXPECT_NEAR(starting.visibility_score, test.want_score, 0.001);
  }
}

TEST(HistoryUrlRowTest, VisitContentAnnotationsMoveSemantics) {
  VisitContentAnnotations original;
  original.model_annotations.categories.emplace_back("cat1", 10);
  original.model_annotations.entities.emplace_back("ent1", 20);
  original.related_searches = {"search1", "search2"};
  original.search_terms = u"test search";
  original.alternative_title = "alt title";
  original.page_language = "en";

  // Verify move constructor transfers contents and empties source containers.
  VisitContentAnnotations moved_constructed(std::move(original));
  EXPECT_EQ(1u, moved_constructed.model_annotations.categories.size());
  EXPECT_EQ("cat1", moved_constructed.model_annotations.categories[0].id);
  EXPECT_EQ(1u, moved_constructed.model_annotations.entities.size());
  EXPECT_EQ(2u, moved_constructed.related_searches.size());
  EXPECT_EQ(u"test search", moved_constructed.search_terms);
  EXPECT_EQ("alt title", moved_constructed.alternative_title);
  EXPECT_EQ("en", moved_constructed.page_language);
  EXPECT_TRUE(original.model_annotations.categories.empty());
  EXPECT_TRUE(original.model_annotations.entities.empty());
  EXPECT_TRUE(original.related_searches.empty());

  // Verify move assignment transfers contents and empties source containers.
  VisitContentAnnotations moved_assigned;
  moved_assigned = std::move(moved_constructed);
  EXPECT_EQ(1u, moved_assigned.model_annotations.categories.size());
  EXPECT_EQ(2u, moved_assigned.related_searches.size());
  EXPECT_EQ("alt title", moved_assigned.alternative_title);
  EXPECT_TRUE(moved_constructed.model_annotations.categories.empty());
  EXPECT_TRUE(moved_constructed.related_searches.empty());
}

TEST(HistoryUrlRowTest, URLResultSetContentAnnotationsMove) {
  URLResult result;
  VisitContentAnnotations annotations;
  annotations.related_searches = {"query1", "query2"};
  annotations.alternative_title = "my page";

  // Pass rvalue into set_content_annotations.
  result.set_content_annotations(std::move(annotations));
  EXPECT_EQ(2u, result.content_annotations().related_searches.size());
  EXPECT_EQ("query1", result.content_annotations().related_searches[0]);
  EXPECT_EQ("my page", result.content_annotations().alternative_title);
  // Moving into the sink setter must empty the original rvalue source.
  EXPECT_TRUE(annotations.related_searches.empty());
}

}  // namespace

}  // namespace history
