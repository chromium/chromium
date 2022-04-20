// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/epoch_topics.h"

#include "base/logging.h"
#include "components/browsing_topics/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_topics {

namespace {

constexpr base::Time kCalculationTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr browsing_topics::HmacKey kTestKey = {1};
constexpr size_t kTaxonomySize = 349;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 2;
constexpr size_t kPaddedTopTopicsStartIndex = 2;

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

TEST_F(EpochTopicsTest, TopicForSite_InvalidIndividualTopics) {
  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  for (int i = 0; i < 5; ++i) {
    top_topics_and_observing_domains.emplace_back(TopicAndDomains());
  }

  EpochTopics epoch_topics(std::move(top_topics_and_observing_domains),
                           kPaddedTopTopicsStartIndex, kTaxonomySize,
                           kTaxonomyVersion, kModelVersion, kCalculationTime);
  EXPECT_FALSE(epoch_topics.empty());

  std::string top_site = "foo.com";

  EXPECT_EQ(epoch_topics.TopicForSiteForDisplay(top_site, kTestKey),
            absl::nullopt);
}

TEST_F(EpochTopicsTest, TopicForSite) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_FALSE(epoch_topics.empty());
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
    {
      bool output_is_true_topic = false;
      bool candidate_topic_filtered = false;
      EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(1), kTestKey,
                                          output_is_true_topic,
                                          candidate_topic_filtered),
                Topic(2));
      EXPECT_TRUE(output_is_true_topic);
      EXPECT_FALSE(candidate_topic_filtered);
    }

    {
      bool output_is_true_topic = false;
      bool candidate_topic_filtered = false;
      EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(2), kTestKey,
                                          output_is_true_topic,
                                          candidate_topic_filtered),
                Topic(2));
      EXPECT_TRUE(output_is_true_topic);
      EXPECT_FALSE(candidate_topic_filtered);
    }

    {
      bool output_is_true_topic = false;
      bool candidate_topic_filtered = false;
      EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(3), kTestKey,
                                          output_is_true_topic,
                                          candidate_topic_filtered),
                absl::nullopt);
      EXPECT_FALSE(output_is_true_topic);
      EXPECT_TRUE(candidate_topic_filtered);
    }

    EXPECT_EQ(epoch_topics.TopicForSiteForDisplay(top_site, kTestKey),
              Topic(2));
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
    {
      bool output_is_true_topic = false;
      bool candidate_topic_filtered = false;
      EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(1), kTestKey,
                                          output_is_true_topic,
                                          candidate_topic_filtered),
                Topic(3));
      EXPECT_FALSE(output_is_true_topic);
      EXPECT_FALSE(candidate_topic_filtered);
    }

    {
      bool output_is_true_topic = false;
      bool candidate_topic_filtered = false;
      EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(2), kTestKey,
                                          output_is_true_topic,
                                          candidate_topic_filtered),
                absl::nullopt);
      EXPECT_FALSE(output_is_true_topic);
      EXPECT_TRUE(candidate_topic_filtered);
    }

    {
      bool output_is_true_topic = false;
      bool candidate_topic_filtered = false;
      EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(3), kTestKey,
                                          output_is_true_topic,
                                          candidate_topic_filtered),
                Topic(3));
      EXPECT_FALSE(output_is_true_topic);
      EXPECT_FALSE(candidate_topic_filtered);
    }

    // Topic(3) is a padded topic. Thus it's not returned.
    EXPECT_EQ(epoch_topics.TopicForSiteForDisplay(top_site, kTestKey),
              absl::nullopt);
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

    bool output_is_true_topic = false;
    bool candidate_topic_filtered = false;
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(1), kTestKey,
                                        output_is_true_topic,
                                        candidate_topic_filtered),
              Topic(186));
    EXPECT_FALSE(output_is_true_topic);
    EXPECT_FALSE(candidate_topic_filtered);
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(2), kTestKey,
                                        output_is_true_topic,
                                        candidate_topic_filtered),
              Topic(186));
    EXPECT_FALSE(output_is_true_topic);
    EXPECT_FALSE(candidate_topic_filtered);
    EXPECT_EQ(epoch_topics.TopicForSite(top_site, HashedDomain(3), kTestKey,
                                        output_is_true_topic,
                                        candidate_topic_filtered),
              Topic(186));
    EXPECT_FALSE(output_is_true_topic);
    EXPECT_FALSE(candidate_topic_filtered);

    EXPECT_EQ(epoch_topics.TopicForSiteForDisplay(top_site, kTestKey),
              absl::nullopt);
  }
}

TEST_F(EpochTopicsTest, ClearTopics) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_FALSE(epoch_topics.empty());

  epoch_topics.ClearTopics();

  EXPECT_TRUE(epoch_topics.empty());

  bool output_is_true_topic = false;
  bool candidate_topic_filtered = false;
  EXPECT_EQ(epoch_topics.TopicForSite(/*top_domain=*/"foo.com", HashedDomain(1),
                                      kTestKey, output_is_true_topic,
                                      candidate_topic_filtered),
            absl::nullopt);
  EXPECT_FALSE(output_is_true_topic);
  EXPECT_FALSE(candidate_topic_filtered);
}

TEST_F(EpochTopicsTest, ClearTopic) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_FALSE(epoch_topics.empty());

  epoch_topics.ClearTopic(Topic(3));

  EXPECT_FALSE(epoch_topics.empty());

  EXPECT_TRUE(epoch_topics.top_topics_and_observing_domains()[0].IsValid());
  EXPECT_TRUE(epoch_topics.top_topics_and_observing_domains()[1].IsValid());
  EXPECT_FALSE(epoch_topics.top_topics_and_observing_domains()[2].IsValid());
  EXPECT_TRUE(epoch_topics.top_topics_and_observing_domains()[3].IsValid());
  EXPECT_TRUE(epoch_topics.top_topics_and_observing_domains()[4].IsValid());
}

TEST_F(EpochTopicsTest, ClearContextDomain) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_FALSE(epoch_topics.empty());

  epoch_topics.ClearContextDomain(HashedDomain(1));

  EXPECT_FALSE(epoch_topics.empty());

  EXPECT_EQ(epoch_topics.top_topics_and_observing_domains()[0].hashed_domains(),
            std::set<HashedDomain>{});
  EXPECT_EQ(epoch_topics.top_topics_and_observing_domains()[1].hashed_domains(),
            std::set<HashedDomain>({HashedDomain(2)}));
  EXPECT_EQ(epoch_topics.top_topics_and_observing_domains()[2].hashed_domains(),
            std::set<HashedDomain>({HashedDomain(3)}));
  EXPECT_EQ(epoch_topics.top_topics_and_observing_domains()[3].hashed_domains(),
            std::set<HashedDomain>({HashedDomain(2), HashedDomain(3)}));
  EXPECT_EQ(epoch_topics.top_topics_and_observing_domains()[4].hashed_domains(),
            std::set<HashedDomain>{});
}

TEST_F(EpochTopicsTest, FromEmptyDictionaryValue) {
  EpochTopics read_epoch_topics =
      EpochTopics::FromDictValue(base::Value::Dict());

  EXPECT_TRUE(read_epoch_topics.empty());
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 0);
  EXPECT_EQ(read_epoch_topics.model_version(), 0);
  EXPECT_EQ(read_epoch_topics.calculation_time(), base::Time());

  bool output_is_true_topic = false;
  bool candidate_topic_filtered = false;
  absl::optional<Topic> topic_for_site = read_epoch_topics.TopicForSite(
      /*top_domain=*/"foo.com",
      /*hashed_context_domain=*/HashedDomain(1), kTestKey, output_is_true_topic,
      candidate_topic_filtered);

  EXPECT_EQ(topic_for_site, absl::nullopt);
}

TEST_F(EpochTopicsTest, EmptyEpochTopics_ToAndFromDictValue) {
  EpochTopics epoch_topics;

  base::Value::Dict dict_value = epoch_topics.ToDictValue();
  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(dict_value);

  EXPECT_TRUE(read_epoch_topics.empty());
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 0);
  EXPECT_EQ(read_epoch_topics.model_version(), 0);
  EXPECT_EQ(read_epoch_topics.calculation_time(), base::Time());

  bool output_is_true_topic = false;
  bool candidate_topic_filtered = false;
  absl::optional<Topic> topic_for_site = read_epoch_topics.TopicForSite(
      /*top_domain=*/"foo.com", HashedDomain(1), kTestKey, output_is_true_topic,
      candidate_topic_filtered);

  EXPECT_EQ(topic_for_site, absl::nullopt);
}

TEST_F(EpochTopicsTest, PopulatedEpochTopics_ToAndFromValue) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  base::Value::Dict dict_value = epoch_topics.ToDictValue();
  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(dict_value);

  EXPECT_FALSE(read_epoch_topics.empty());
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 1);
  EXPECT_EQ(read_epoch_topics.model_version(), 2);
  EXPECT_EQ(read_epoch_topics.calculation_time(), kCalculationTime);

  bool output_is_true_topic = false;
  bool candidate_topic_filtered = false;
  EXPECT_EQ(read_epoch_topics.TopicForSite(
                /*top_domain=*/"foo.com", HashedDomain(1), kTestKey,
                output_is_true_topic, candidate_topic_filtered),
            Topic(2));
}

}  // namespace browsing_topics
