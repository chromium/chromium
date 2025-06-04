// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/insertion_ordered_set.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(InsertionOrderedSetTest, InsertOneElement) {
  InsertionOrderedSet<int> set;
  set.insert(1);
  EXPECT_EQ(1u, set.set().size());
  EXPECT_EQ(1u, set.vector().size());
  EXPECT_TRUE(set.set().contains(1));
  EXPECT_EQ(1, set.vector()[0]);
}

TEST(InsertionOrderedSetTest, InsertTwoDistinctElements) {
  InsertionOrderedSet<int> set;
  set.insert(1);
  set.insert(2);
  EXPECT_EQ(2u, set.set().size());
  EXPECT_EQ(2u, set.vector().size());
  EXPECT_TRUE(set.set().contains(1));
  EXPECT_TRUE(set.set().contains(2));
  EXPECT_EQ(1, set.vector()[0]);
  EXPECT_EQ(2, set.vector()[1]);
}

TEST(InsertionOrderedSetTest, InsertTwoIdenticalElements) {
  InsertionOrderedSet<int> set;
  set.insert(1);
  set.insert(1);
  EXPECT_EQ(1u, set.set().size());
  EXPECT_EQ(1u, set.vector().size());
  EXPECT_TRUE(set.set().contains(1));
  EXPECT_EQ(1, set.vector()[0]);
}

TEST(InsertionOrderedSetTest, Empty) {
  InsertionOrderedSet<int> set;
  EXPECT_TRUE(set.empty());
  set.insert(1);
  EXPECT_FALSE(set.empty());
}

TEST(InsertionOrderedSetTest, Clear) {
  InsertionOrderedSet<int> set;
  set.insert(1);
  EXPECT_FALSE(set.empty());
  set.clear();
  EXPECT_TRUE(set.empty());
}

}  // namespace optimization_guide
