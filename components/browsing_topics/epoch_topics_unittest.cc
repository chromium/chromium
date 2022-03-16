// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/epoch_topics.h"

#include "base/logging.h"
#include "components/browsing_topics/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_topics {

namespace {

const base::Time kCalculationTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
const browsing_topics::HmacKey kTestKey = {1};
const size_t kTaxonomySize = 349;
const int kTaxonomyVersion = 1;
const int kModelVersion = 2;
const size_t kPaddedTopTopicsStartIndex = 3;

EpochTopics CreateTestEpochTopics() {
  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(1), {HashedDomain(1)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(2), {HashedDomain(1), HashedDomain(2)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(3), {HashedDomain(1), HashedDomain(3)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(4), {HashedDomain(2), HashedDomain(3)}));
  top_topics_and_observing_domains.emplace_back(
      TopicAndDomains(Topic(5), {HashedDomain(1)}));

  EpochTopics epoch_topics(std::move(top_topics_and_observing_domains),
                           kPaddedTopTopicsStartIndex, kTaxonomySize,
                           kTaxonomyVersion, kModelVersion, kCalculationTime);

  return epoch_topics;
}

}  // namespace

class EpochTopicsTest : public testing::Test {};

TEST_F(EpochTopicsTest, TopicForSite) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_TRUE(epoch_topics.HasValidTopics());
  EXPECT_EQ(epoch_topics.taxonomy_version(), kTaxonomyVersion);
  EXPECT_EQ(epoch_topics.model_version(), kModelVersion);
  EXPECT_EQ(epoch_topics.calculation_time(), kCalculationTime);

  {
    std::string top_site = "foo.com";
    uint64_t random_or_top_topic_decision_hash =
        HashTopDomainForRandomOrTopTopicDecision(kTestKey, kCalculationTime,
                                                 top_site);

    // `random_or_top_topic_decision_hash` mod 100 is not less than 5. Thus one
    // of the top 5 topics will be the candidate topic.
    ASSERT_GE(random_or_top_topic_decision_hash % 100, 5ULL);

    uint64_t top_topics_index_decision_hash =
        HashTopDomainForTopTopicIndexDecision(kTestKey, kCalculationTime,
                                              top_site);

    // The topic index is 1, thus the candidate topic is Topic(2). Only the
    // context with HashedDomain(1) or HashedDomain(2) is allowed to see it.
    ASSERT_EQ(top_topics_index_decision_hash % 5, 1ULL);

    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(1), kTestKey),
              Topic(2));
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(2), kTestKey),
              Topic(2));
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(3), kTestKey),
              absl::nullopt);
  }

  {
    std::string top_site = "foo1.com";
    uint64_t random_or_top_topic_decision_hash =
        HashTopDomainForRandomOrTopTopicDecision(kTestKey, kCalculationTime,
                                                 top_site);

    // `random_or_top_topic_decision_hash` mod 100 is not less than 5. Thus one
    // of the top 5 topics will be the candidate topic.
    ASSERT_GE(random_or_top_topic_decision_hash % 100, 5ULL);

    uint64_t top_topics_index_decision_hash =
        HashTopDomainForTopTopicIndexDecision(kTestKey, kCalculationTime,
                                              top_site);

    // The topic index is 2, thus the candidate topic is Topic(3). Only the
    // context with HashedDomain(1) or HashedDomain(3) is allowed to see it.
    ASSERT_EQ(top_topics_index_decision_hash % 5, 2ULL);

    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(1), kTestKey),
              Topic(3));
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(2), kTestKey),
              absl::nullopt);
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(3), kTestKey),
              Topic(3));
  }

  {
    std::string top_site = "foo5.com";
    uint64_t random_or_top_topic_decision_hash =
        HashTopDomainForRandomOrTopTopicDecision(kTestKey, kCalculationTime,
                                                 top_site);

    // `random_or_top_topic_decision_hash` mod 100 is less than 5. Thus the
    // random topic will be returned.
    ASSERT_LT(random_or_top_topic_decision_hash % 100, 5ULL);

    uint64_t random_topic_index_decision =
        HashTopDomainForRandomTopicIndexDecision(kTestKey, kCalculationTime,
                                                 top_site);

    // The random topic index is 185, thus Topic(186) will be returned.
    ASSERT_EQ(random_topic_index_decision % kTaxonomySize, 185ULL);

    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(1), kTestKey),
              Topic(186));
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(2), kTestKey),
              Topic(186));
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(3), kTestKey),
              Topic(186));
  }
}

TEST_F(EpochTopicsTest, ClearTopics) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_TRUE(epoch_topics.HasValidTopics());

  epoch_topics.ClearTopics();

  EXPECT_FALSE(epoch_topics.HasValidTopics());

  EXPECT_EQ(epoch_topics.TopicForSite(/*top_domain=*/"foo.com", HashedDomain(1),
                                      kTestKey),
            absl::nullopt);
}

TEST_F(EpochTopicsTest, FromEmptyDictionaryValue) {
  EpochTopics read_epoch_topics =
      EpochTopics::FromDictValue(base::Value::Dict());

  EXPECT_FALSE(read_epoch_topics.HasValidTopics());
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 0);
  EXPECT_EQ(read_epoch_topics.model_version(), 0);
  EXPECT_EQ(read_epoch_topics.calculation_time(), base::Time());

  absl::optional<Topic> topic_for_site = read_epoch_topics.TopicForSite(
      /*top_domain=*/"foo.com",
      /*hashed_context_domain=*/HashedDomain(1), kTestKey);

  EXPECT_EQ(topic_for_site, absl::nullopt);
}

TEST_F(EpochTopicsTest, EmptyEpochTopics_ToAndFromDictValue) {
  EpochTopics epoch_topics;

  base::Value::Dict dict_value = epoch_topics.ToDictValue();
  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(dict_value);

  EXPECT_FALSE(read_epoch_topics.HasValidTopics());
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 0);
  EXPECT_EQ(read_epoch_topics.model_version(), 0);
  EXPECT_EQ(read_epoch_topics.calculation_time(), base::Time());

  absl::optional<Topic> topic_for_site = read_epoch_topics.TopicForSite(
      /*top_domain=*/"foo.com", HashedDomain(1), kTestKey);

  EXPECT_EQ(topic_for_site, absl::nullopt);
}

TEST_F(EpochTopicsTest, PopulatedEpochTopics_ToAndFromValue) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  base::Value::Dict dict_value = epoch_topics.ToDictValue();
  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(dict_value);

  EXPECT_TRUE(read_epoch_topics.HasValidTopics());
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 1);
  EXPECT_EQ(read_epoch_topics.model_version(), 2);
  EXPECT_EQ(read_epoch_topics.calculation_time(), kCalculationTime);

  EXPECT_EQ(read_epoch_topics.TopicForSite(/*top_domain=*/"foo.com",
                                           HashedDomain(1), kTestKey),
            Topic(2));
}

}  // namespace browsing_topics
