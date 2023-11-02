// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/topic_invalidation_map.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

namespace {

// TODO(crbug.com/1056651): some methods aren't covered by tests, it's likely
// worth adding them (especially for ToValue(), which is passed to js code).
class TopicInvalidationMapTest : public testing::Test {
 public:
  TopicInvalidationMapTest()
      : kTopicOne("one"),
        kTopicTwo("two"),
        kInv1(Invalidation::Init(kTopicOne, 10, "ten")) {
    set1_.insert(kTopicOne);
    set2_.insert(kTopicTwo);
    all_set_.insert(kTopicOne);
    all_set_.insert(kTopicTwo);

    one_invalidation_.Insert(kInv1);
    invalidate_all_.Insert(Invalidation::InitUnknownVersion(kTopicOne));
    invalidate_all_.Insert(Invalidation::InitUnknownVersion(kTopicTwo));
  }

  ~TopicInvalidationMapTest() override = default;

 protected:
  const Topic kTopicOne;
  const Topic kTopicTwo;
  const Invalidation kInv1;

  TopicSet set1_;
  TopicSet set2_;
  TopicSet all_set_;
  TopicInvalidationMap empty_;
  TopicInvalidationMap one_invalidation_;
  TopicInvalidationMap invalidate_all_;
};

TEST_F(TopicInvalidationMapTest, Empty) {
  EXPECT_TRUE(empty_.Empty());
  EXPECT_FALSE(one_invalidation_.Empty());
  EXPECT_FALSE(invalidate_all_.Empty());
}

TEST_F(TopicInvalidationMapTest, Equality) {
  // TODO(crbug.com/1056651): equality operator is only used in tests, so maybe
  // factor it away from the TopicInvalidationMap.
  TopicInvalidationMap empty2;
  EXPECT_EQ(empty_, empty2);

  TopicInvalidationMap one_invalidation_2;
  one_invalidation_2.Insert(kInv1);
  EXPECT_EQ(one_invalidation_, one_invalidation_2);

  EXPECT_FALSE(empty_ == invalidate_all_);
}

TEST_F(TopicInvalidationMapTest, GetTopics) {
  EXPECT_EQ(TopicSet(), empty_.GetTopics());
  EXPECT_EQ(set1_, one_invalidation_.GetTopics());
  EXPECT_EQ(all_set_, invalidate_all_.GetTopics());
}

TEST_F(TopicInvalidationMapTest, GetSubsetWithTopics) {
  EXPECT_TRUE(empty_.GetSubsetWithTopics(set1_).Empty());

  EXPECT_EQ(one_invalidation_.GetSubsetWithTopics(set1_), one_invalidation_);
  EXPECT_EQ(one_invalidation_.GetSubsetWithTopics(all_set_), one_invalidation_);
  EXPECT_TRUE(one_invalidation_.GetSubsetWithTopics(set2_).Empty());

  EXPECT_TRUE(invalidate_all_.GetSubsetWithTopics(TopicSet()).Empty());
}

}  // namespace

}  // namespace invalidation
