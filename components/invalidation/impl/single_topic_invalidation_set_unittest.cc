// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/single_topic_invalidation_set.h"

#include <memory>

#include "components/invalidation/impl/invalidation_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

namespace {

class SingleTopicInvalidationSetTest : public testing::Test {
 public:
  SingleTopicInvalidationSetTest() = default;

 protected:
  const Topic kTopic = "one";
};

TEST_F(SingleTopicInvalidationSetTest, InsertionAndOrdering) {
  SingleTopicInvalidationSet l1;
  SingleTopicInvalidationSet l2;

  Invalidation inv0 = Invalidation::InitUnknownVersion(kTopic);
  Invalidation inv1 = Invalidation::Init(kTopic, 1, "one");
  Invalidation inv2 = Invalidation::Init(kTopic, 5, "five");

  l1.Insert(inv0);
  l1.Insert(inv1);
  l1.Insert(inv2);

  l2.Insert(inv1);
  l2.Insert(inv2);
  l2.Insert(inv0);

  ASSERT_EQ(3U, l1.GetSize());
  ASSERT_EQ(3U, l2.GetSize());

  auto it1 = l1.begin();
  auto it2 = l2.begin();
  EXPECT_THAT(inv0, Eq(*it1));
  EXPECT_THAT(inv0, Eq(*it2));
  it1++;
  it2++;
  EXPECT_THAT(inv1, Eq(*it1));
  EXPECT_THAT(inv1, Eq(*it2));
  it1++;
  it2++;
  EXPECT_THAT(inv2, Eq(*it1));
  EXPECT_THAT(inv2, Eq(*it2));
  it1++;
  it2++;
  EXPECT_TRUE(it1 == l1.end());
  EXPECT_TRUE(it2 == l2.end());
}

TEST_F(SingleTopicInvalidationSetTest, StartWithUnknownVersion) {
  SingleTopicInvalidationSet list;
  EXPECT_FALSE(list.StartsWithUnknownVersion());

  list.Insert(Invalidation::Init(kTopic, 1, "one"));
  EXPECT_FALSE(list.StartsWithUnknownVersion());

  list.Insert(Invalidation::InitUnknownVersion(kTopic));
  EXPECT_TRUE(list.StartsWithUnknownVersion());

  list.Clear();
  EXPECT_FALSE(list.StartsWithUnknownVersion());
}

}  // namespace

}  // namespace invalidation
