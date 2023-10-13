// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/single_topic_invalidation_set.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

namespace {

class SingleTopicInvalidationSetTest : public testing::Test {
 protected:
  const Topic kTopic = "one";
};

TEST_F(SingleTopicInvalidationSetTest, InsertionAndOrdering) {
  SingleTopicInvalidationSet l1;
  SingleTopicInvalidationSet l2;

  Invalidation inv1 = Invalidation::Init(kTopic, 1, "one");
  Invalidation inv2 = Invalidation::Init(kTopic, 5, "five");

  l1.Insert(inv1);
  l1.Insert(inv2);

  l2.Insert(inv2);
  l2.Insert(inv1);

  ASSERT_EQ(2U, l1.GetSize());
  ASSERT_EQ(2U, l2.GetSize());

  auto it1 = l1.begin();
  auto it2 = l2.begin();
  EXPECT_EQ(inv1, *it1);
  EXPECT_EQ(inv1, *it2);
  it1++;
  it2++;
  EXPECT_EQ(inv2, *it1);
  EXPECT_EQ(inv2, *it2);
  it1++;
  it2++;
  EXPECT_EQ(it1, l1.end());
  EXPECT_EQ(it2, l2.end());
}

}  // namespace
}  // namespace invalidation
