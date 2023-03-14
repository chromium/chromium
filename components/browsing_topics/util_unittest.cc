// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/util.h"

#include "base/test/gtest_util.h"

namespace browsing_topics {

class BrowsingTopicsUtilTest : public testing::Test {};

TEST_F(BrowsingTopicsUtilTest, GetDescendantTopicsEmptyMap) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_TRUE(topics.empty());
}

TEST_F(BrowsingTopicsUtilTest, GetDescendantTopicsTopicNotInMap) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  parent_child_map[Topic(2)] = {Topic(3), Topic(1)};
  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_TRUE(topics.empty());
}

TEST_F(BrowsingTopicsUtilTest, GetDescendantTopicsEmptyChildrenList) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  parent_child_map[Topic(1)] = {};
  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_TRUE(topics.empty());
}

TEST_F(BrowsingTopicsUtilTest, GetDescendantTopicsOneChild) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  parent_child_map[Topic(1)] = {Topic(2)};
  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_FALSE(topics.empty());
  std::set<Topic> expected_descendants = {Topic(2)};
  EXPECT_EQ(topics, expected_descendants);
}

TEST_F(BrowsingTopicsUtilTest, GetDescendantTopicsMultipleChildren) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  parent_child_map[Topic(1)] = {Topic(2), Topic(3)};
  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_FALSE(topics.empty());
  std::set<Topic> expected_descendants = {Topic(2), Topic(3)};
  EXPECT_EQ(topics, expected_descendants);
}

TEST_F(BrowsingTopicsUtilTest, GetDescendantTopicsMultipleLevelsOfDescendants) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  parent_child_map[Topic(1)] = {Topic(2), Topic(3)};
  parent_child_map[Topic(2)] = {Topic(4), Topic(5)};
  parent_child_map[Topic(3)] = {Topic(6), Topic(7)};
  parent_child_map[Topic(4)] = {Topic(8)};
  parent_child_map[Topic(8)] = {Topic(9)};
  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_FALSE(topics.empty());
  std::set<Topic> expected_descendants = {Topic(2), Topic(3), Topic(4),
                                          Topic(5), Topic(6), Topic(7),
                                          Topic(8), Topic(9)};
  EXPECT_EQ(topics, expected_descendants);
}

TEST_F(BrowsingTopicsUtilTest, GetDescendantTopicsMapContainsNonDescendants) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  parent_child_map[Topic(1)] = {Topic(2), Topic(3)};
  parent_child_map[Topic(2)] = {Topic(4), Topic(5)};
  parent_child_map[Topic(8)] = {Topic(9)};
  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_FALSE(topics.empty());
  std::set<Topic> expected_descendants = {Topic(2), Topic(3), Topic(4),
                                          Topic(5)};
  EXPECT_EQ(topics, expected_descendants);
}

TEST_F(BrowsingTopicsUtilTest,
       GetDescendantTopicsMapDescendantsMultipleParents) {
  std::map<Topic, std::vector<Topic>> parent_child_map;
  parent_child_map[Topic(1)] = {Topic(2), Topic(3)};
  parent_child_map[Topic(8)] = {Topic(2), Topic(3)};
  parent_child_map[Topic(2)] = {Topic(4), Topic(5)};
  parent_child_map[Topic(3)] = {Topic(4), Topic(5)};

  std::set<Topic> topics = GetDescendantTopics(Topic(1), parent_child_map);
  EXPECT_FALSE(topics.empty());
  std::set<Topic> expected_descendants = {Topic(2), Topic(3), Topic(4),
                                          Topic(5)};
  EXPECT_EQ(topics, expected_descendants);
}

}  // namespace browsing_topics
