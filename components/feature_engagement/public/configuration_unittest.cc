// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/configuration.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

TEST(ComparatorTest, Any) {
  EXPECT_TRUE(Comparator(ANY, 0).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(ANY, 1).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(ANY, 1).MeetsCriteria(1));
  EXPECT_TRUE(Comparator(ANY, 1).MeetsCriteria(2));
  EXPECT_TRUE(Comparator(ANY, 10).MeetsCriteria(9));
  EXPECT_TRUE(Comparator(ANY, 10).MeetsCriteria(10));
  EXPECT_TRUE(Comparator(ANY, 10).MeetsCriteria(11));
}

TEST(ComparatorTest, LessThan) {
  EXPECT_FALSE(Comparator(LESS_THAN, 0).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(LESS_THAN, 1).MeetsCriteria(0));
  EXPECT_FALSE(Comparator(LESS_THAN, 1).MeetsCriteria(1));
  EXPECT_FALSE(Comparator(LESS_THAN, 1).MeetsCriteria(2));
  EXPECT_TRUE(Comparator(LESS_THAN, 10).MeetsCriteria(9));
  EXPECT_FALSE(Comparator(LESS_THAN, 10).MeetsCriteria(10));
  EXPECT_FALSE(Comparator(LESS_THAN, 10).MeetsCriteria(11));
}

TEST(ComparatorTest, GreaterThan) {
  EXPECT_FALSE(Comparator(GREATER_THAN, 0).MeetsCriteria(0));
  EXPECT_FALSE(Comparator(GREATER_THAN, 1).MeetsCriteria(0));
  EXPECT_FALSE(Comparator(GREATER_THAN, 1).MeetsCriteria(1));
  EXPECT_TRUE(Comparator(GREATER_THAN, 1).MeetsCriteria(2));
  EXPECT_FALSE(Comparator(GREATER_THAN, 10).MeetsCriteria(9));
  EXPECT_FALSE(Comparator(GREATER_THAN, 10).MeetsCriteria(10));
  EXPECT_TRUE(Comparator(GREATER_THAN, 10).MeetsCriteria(11));
}

TEST(ComparatorTest, LessThanOrEqual) {
  EXPECT_TRUE(Comparator(LESS_THAN_OR_EQUAL, 0).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(LESS_THAN_OR_EQUAL, 1).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(LESS_THAN_OR_EQUAL, 1).MeetsCriteria(1));
  EXPECT_FALSE(Comparator(LESS_THAN_OR_EQUAL, 1).MeetsCriteria(2));
  EXPECT_TRUE(Comparator(LESS_THAN_OR_EQUAL, 10).MeetsCriteria(9));
  EXPECT_TRUE(Comparator(LESS_THAN_OR_EQUAL, 10).MeetsCriteria(10));
  EXPECT_FALSE(Comparator(LESS_THAN_OR_EQUAL, 10).MeetsCriteria(11));
}

TEST(ComparatorTest, GreaterThanOrEqual) {
  EXPECT_TRUE(Comparator(GREATER_THAN_OR_EQUAL, 0).MeetsCriteria(0));
  EXPECT_FALSE(Comparator(GREATER_THAN_OR_EQUAL, 1).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(GREATER_THAN_OR_EQUAL, 1).MeetsCriteria(1));
  EXPECT_TRUE(Comparator(GREATER_THAN_OR_EQUAL, 1).MeetsCriteria(2));
  EXPECT_FALSE(Comparator(GREATER_THAN_OR_EQUAL, 10).MeetsCriteria(9));
  EXPECT_TRUE(Comparator(GREATER_THAN_OR_EQUAL, 10).MeetsCriteria(10));
  EXPECT_TRUE(Comparator(GREATER_THAN_OR_EQUAL, 10).MeetsCriteria(11));
}

TEST(ComparatorTest, Equal) {
  EXPECT_TRUE(Comparator(EQUAL, 0).MeetsCriteria(0));
  EXPECT_FALSE(Comparator(EQUAL, 1).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(EQUAL, 1).MeetsCriteria(1));
  EXPECT_FALSE(Comparator(EQUAL, 1).MeetsCriteria(2));
  EXPECT_FALSE(Comparator(EQUAL, 10).MeetsCriteria(9));
  EXPECT_TRUE(Comparator(EQUAL, 10).MeetsCriteria(10));
  EXPECT_FALSE(Comparator(EQUAL, 10).MeetsCriteria(11));
}

TEST(ComparatorTest, NotEqual) {
  EXPECT_FALSE(Comparator(NOT_EQUAL, 0).MeetsCriteria(0));
  EXPECT_TRUE(Comparator(NOT_EQUAL, 1).MeetsCriteria(0));
  EXPECT_FALSE(Comparator(NOT_EQUAL, 1).MeetsCriteria(1));
  EXPECT_TRUE(Comparator(NOT_EQUAL, 1).MeetsCriteria(2));
  EXPECT_TRUE(Comparator(NOT_EQUAL, 10).MeetsCriteria(9));
  EXPECT_FALSE(Comparator(NOT_EQUAL, 10).MeetsCriteria(10));
  EXPECT_TRUE(Comparator(NOT_EQUAL, 10).MeetsCriteria(11));
}

}  // namespace feature_engagement
