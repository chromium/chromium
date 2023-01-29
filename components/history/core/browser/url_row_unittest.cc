// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_row.h"

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

}  // namespace

}  // namespace history
