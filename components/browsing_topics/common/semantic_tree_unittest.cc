// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/common/semantic_tree.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

class SemanticTreeUnittest : public testing::Test {
 protected:
  SemanticTree semantic_tree_;
};

TEST_F(SemanticTreeUnittest, GetDescendantTopicsTopicNotInMap) {
  std::vector<Topic> topics = semantic_tree_.GetDescendantTopics(Topic(10000));
  EXPECT_TRUE(topics.empty());
}

TEST_F(SemanticTreeUnittest, GetDescendantTopicsNoChildren) {
  std::vector<Topic> topics = semantic_tree_.GetDescendantTopics(Topic(8));
  EXPECT_TRUE(topics.empty());
}

TEST_F(SemanticTreeUnittest, GetDescendantTopicsOneChild) {
  std::vector<Topic> topics = semantic_tree_.GetDescendantTopics(Topic(7));
  std::vector<Topic> expected_descendants = {Topic(8)};
  EXPECT_EQ(topics, expected_descendants);
}

TEST_F(SemanticTreeUnittest, GetDescendantTopicsMultipleChildren) {
  std::vector<Topic> topics = semantic_tree_.GetDescendantTopics(Topic(250));
  std::vector<Topic> expected_descendants = {Topic(251), Topic(252),
                                             Topic(253)};
  EXPECT_EQ(topics, expected_descendants);
}

TEST_F(SemanticTreeUnittest, GetDescendantTopicsMultipleLevelsOfDescendants) {
  std::vector<Topic> topics = semantic_tree_.GetDescendantTopics(Topic(255));
  std::vector<Topic> expected_descendants = {
      Topic(256), Topic(257), Topic(258), Topic(259), Topic(260), Topic(261),
  };
  EXPECT_EQ(topics, expected_descendants);
}

TEST_F(SemanticTreeUnittest, GetAncestorTopicsTopicNotInMap) {
  std::vector<Topic> topics = semantic_tree_.GetAncestorTopics(Topic(10000));
  EXPECT_TRUE(topics.empty());
}

TEST_F(SemanticTreeUnittest, GetAncestorTopicsNoAncestors) {
  std::vector<Topic> topics = semantic_tree_.GetAncestorTopics(Topic(1));
  EXPECT_TRUE(topics.empty());
}

TEST_F(SemanticTreeUnittest, GetAncestorTopicsOneAncestor) {
  std::vector<Topic> topics = semantic_tree_.GetAncestorTopics(Topic(2));
  EXPECT_FALSE(topics.empty());
  std::vector<Topic> expected_ancestors = {Topic(1)};
  EXPECT_EQ(topics, expected_ancestors);
}

TEST_F(SemanticTreeUnittest, GetAncestorTopicsMultipleAncestors) {
  std::vector<Topic> topics = semantic_tree_.GetAncestorTopics(Topic(36));
  EXPECT_FALSE(topics.empty());
  std::vector<Topic> expected_ancestors = {Topic(33), Topic(23), Topic(1)};
  EXPECT_EQ(topics, expected_ancestors);
}

TEST_F(SemanticTreeUnittest, GetLatestLocalizedNameMessageIdValidTopic) {
  absl::optional<int> message_id =
      semantic_tree_.GetLatestLocalizedNameMessageId(Topic(100));
  EXPECT_EQ(message_id.value(),
            IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_100);
}

TEST_F(SemanticTreeUnittest, GetLatestLocalizedNameMessageIdInvalidTaxonomy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kBrowsingTopics, {{"taxonomy_version", "0"}});
  absl::optional<int> message_id =
      semantic_tree_.GetLatestLocalizedNameMessageId(Topic(100));
  EXPECT_FALSE(message_id.has_value());
}

TEST_F(SemanticTreeUnittest, GetLatestLocalizedNameMessageIdInvalidTopic) {
  absl::optional<int> message_id =
      semantic_tree_.GetLatestLocalizedNameMessageId(Topic(9999));
  EXPECT_FALSE(message_id.has_value());
}
}  // namespace browsing_topics
