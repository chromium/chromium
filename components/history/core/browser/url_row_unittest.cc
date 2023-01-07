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

}  // namespace

}  // namespace history
