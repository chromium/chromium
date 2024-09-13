// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/epoch_topics.h"

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browsing_topics/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

constexpr base::Time kCalculationTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr browsing_topics::HmacKey kTestKey = {1};
constexpr size_t kTaxonomySize = 349;
constexpr int kConfigVersion = 1;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 2;
constexpr size_t kPaddedTopTopicsStartIndex = 2;

std::vector<TopicAndDomains> CreateTestTopTopics() {
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
      TopicAndDomains(Topic(100), {HashedDomain(1)}));
  return top_topics_and_observing_domains;
}

EpochTopics CreateTestEpochTopics(
    base::Time calculation_time = kCalculationTime) {
  EpochTopics epoch_topics(CreateTestTopTopics(), kPaddedTopTopicsStartIndex,
                           kConfigVersion, kTaxonomyVersion, kModelVersion,
                           calculation_time,
                           /*from_manually_triggered_calculation=*/true);

  return epoch_topics;
}

}  // namespace

class EpochTopicsTest : public testing::Test {
 public:
  EpochTopicsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(EpochTopicsTest, CandidateTopicForSite_InvalidIndividualTopics) {
  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  for (int i = 0; i < 5; ++i) {
    top_topics_and_observing_domains.emplace_back(TopicAndDomains());
  }

  EpochTopics epoch_topics(std::move(top_topics_and_observing_domains),
                           kPaddedTopTopicsStartIndex, kConfigVersion,
                           kTaxonomyVersion, kModelVersion, kCalculationTime,
                           /*from_manually_triggered_calculation=*/false);
  EXPECT_FALSE(epoch_topics.empty());

  CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
      /*top_domain=*/"foo.com", /*hashed_context_domain=*/HashedDomain(2),
      kTestKey);
  EXPECT_FALSE(candidate_topic.IsValid());
}

TEST_F(EpochTopicsTest, CandidateTopicForSite) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_FALSE(epoch_topics.empty());
  EXPECT_EQ(epoch_topics.config_version(), kConfigVersion);
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
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(1), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(2));
      EXPECT_TRUE(candidate_topic.is_true_topic());
      EXPECT_FALSE(candidate_topic.should_be_filtered());
    }

    {
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(2), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(2));
      EXPECT_TRUE(candidate_topic.is_true_topic());
      EXPECT_FALSE(candidate_topic.should_be_filtered());
    }

    {
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(3), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(2));
      EXPECT_TRUE(candidate_topic.is_true_topic());
      EXPECT_TRUE(candidate_topic.should_be_filtered());
    }
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
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(1), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(3));
      EXPECT_FALSE(candidate_topic.is_true_topic());
      EXPECT_FALSE(candidate_topic.should_be_filtered());
    }

    {
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(2), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(3));
      EXPECT_FALSE(candidate_topic.is_true_topic());
      EXPECT_TRUE(candidate_topic.should_be_filtered());
    }

    {
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(3), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(3));
      EXPECT_FALSE(candidate_topic.is_true_topic());
      EXPECT_FALSE(candidate_topic.should_be_filtered());
    }
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

    // The real topic would have been 4, but a random topic (186) is returned
    // instead. Only callers that are able to receive 4 (domains 2 and 3) should
    // receive the random topic.
    ASSERT_EQ(random_topic_index_decision % kTaxonomySize, 185ULL);

    {
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(1), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(186));
      EXPECT_FALSE(candidate_topic.is_true_topic());
      EXPECT_TRUE(candidate_topic.should_be_filtered());
    }

    {
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(2), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(186));
      EXPECT_FALSE(candidate_topic.is_true_topic());
      EXPECT_FALSE(candidate_topic.should_be_filtered());
    }

    {
      CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
          top_site, HashedDomain(3), kTestKey);

      EXPECT_EQ(candidate_topic.topic(), Topic(186));
      EXPECT_FALSE(candidate_topic.is_true_topic());
      EXPECT_FALSE(candidate_topic.should_be_filtered());
    }
  }
}

TEST_F(EpochTopicsTest, ClearTopics) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_FALSE(epoch_topics.empty());

  epoch_topics.ClearTopics();

  EXPECT_TRUE(epoch_topics.empty());

  CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
      /*top_domain=*/"foo.com", HashedDomain(1), kTestKey);

  EXPECT_FALSE(candidate_topic.IsValid());
}

TEST_F(EpochTopicsTest, ClearTopic_NoDescendants) {
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

TEST_F(EpochTopicsTest, ClearTopic_WithDescendants) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  EXPECT_FALSE(epoch_topics.empty());

  epoch_topics.ClearTopic(Topic(1));

  EXPECT_FALSE(epoch_topics.empty());

  EXPECT_FALSE(epoch_topics.top_topics_and_observing_domains()[0].IsValid());
  EXPECT_FALSE(epoch_topics.top_topics_and_observing_domains()[1].IsValid());
  EXPECT_FALSE(epoch_topics.top_topics_and_observing_domains()[2].IsValid());
  EXPECT_FALSE(epoch_topics.top_topics_and_observing_domains()[3].IsValid());
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
  EXPECT_EQ(read_epoch_topics.config_version(), 0);
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 0);
  EXPECT_EQ(read_epoch_topics.model_version(), 0);
  EXPECT_EQ(read_epoch_topics.calculation_time(), base::Time());

  CandidateTopic candidate_topic = read_epoch_topics.CandidateTopicForSite(
      /*top_domain=*/"foo.com", HashedDomain(1), kTestKey);

  EXPECT_FALSE(candidate_topic.IsValid());
}

TEST_F(EpochTopicsTest,
       FromDictionaryValueWithoutConfigVersion_UseConfigVersion1) {
  base::Value::Dict dict;

  base::Value::List top_topics_and_observing_domains_list;
  std::vector<TopicAndDomains> top_topics_and_domains = CreateTestTopTopics();
  for (const TopicAndDomains& topic_and_domains : top_topics_and_domains) {
    top_topics_and_observing_domains_list.Append(
        topic_and_domains.ToDictValue());
  }

  dict.Set("top_topics_and_observing_domains",
           std::move(top_topics_and_observing_domains_list));
  dict.Set("padded_top_topics_start_index", 0);
  dict.Set("taxonomy_version", 2);
  dict.Set("model_version", base::Int64ToValue(3));
  dict.Set("calculation_time", base::TimeToValue(kCalculationTime));

  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(std::move(dict));

  EXPECT_FALSE(read_epoch_topics.empty());
  EXPECT_EQ(read_epoch_topics.config_version(), 1);
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 2);
  EXPECT_EQ(read_epoch_topics.model_version(), 3);
  EXPECT_EQ(read_epoch_topics.calculation_time(), kCalculationTime);
}

TEST_F(EpochTopicsTest, EmptyEpochTopics_ToAndFromDictValue) {
  EpochTopics epoch_topics(kCalculationTime);

  base::Value::Dict dict_value = epoch_topics.ToDictValue();
  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(dict_value);

  EXPECT_TRUE(read_epoch_topics.empty());
  EXPECT_EQ(read_epoch_topics.config_version(), 0);
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 0);
  EXPECT_EQ(read_epoch_topics.model_version(), 0);
  EXPECT_EQ(read_epoch_topics.calculation_time(), kCalculationTime);
  EXPECT_FALSE(read_epoch_topics.calculator_result_status());

  CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
      /*top_domain=*/"foo.com", HashedDomain(1), kTestKey);

  EXPECT_FALSE(candidate_topic.IsValid());
}

TEST_F(EpochTopicsTest, PopulatedEpochTopics_ToAndFromValue) {
  EpochTopics epoch_topics = CreateTestEpochTopics();

  base::Value::Dict dict_value = epoch_topics.ToDictValue();
  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(dict_value);

  EXPECT_FALSE(read_epoch_topics.empty());
  EXPECT_EQ(read_epoch_topics.config_version(), 1);
  EXPECT_EQ(read_epoch_topics.taxonomy_version(), 1);
  EXPECT_EQ(read_epoch_topics.model_version(), 2);
  EXPECT_EQ(read_epoch_topics.calculation_time(), kCalculationTime);

  // `from_manually_triggered_calculation` should not persist after being
  // written.
  EXPECT_TRUE(epoch_topics.from_manually_triggered_calculation());
  EXPECT_FALSE(read_epoch_topics.from_manually_triggered_calculation());

  // The kSuccess `calculator_result_status` should persist after being written.
  EXPECT_EQ(epoch_topics.calculator_result_status(),
            CalculatorResultStatus::kSuccess);
  EXPECT_EQ(read_epoch_topics.calculator_result_status(),
            CalculatorResultStatus::kSuccess);

  CandidateTopic candidate_topic = epoch_topics.CandidateTopicForSite(
      /*top_domain=*/"foo.com", HashedDomain(1), kTestKey);

  EXPECT_EQ(candidate_topic.topic(), Topic(2));
}

TEST_F(EpochTopicsTest,
       EmptyEpochTopicsWithCalculatorResultStatus_ToAndFromDictValue) {
  EpochTopics epoch_topics(
      kCalculationTime,
      CalculatorResultStatus::kFailureAnnotationExecutionError);

  base::Value::Dict dict_value = epoch_topics.ToDictValue();
  EpochTopics read_epoch_topics = EpochTopics::FromDictValue(dict_value);

  EXPECT_TRUE(read_epoch_topics.empty());

  // The failure `calculator_result_status` should persist after being written.
  EXPECT_EQ(epoch_topics.calculator_result_status(),
            CalculatorResultStatus::kFailureAnnotationExecutionError);
  EXPECT_FALSE(read_epoch_topics.calculator_result_status());
}

TEST_F(EpochTopicsTest, ScheduleExpiration) {
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{blink::features::kBrowsingTopics, {}},
       {blink::features::kBrowsingTopicsParameters,
        {{"epoch_retention_duration", "28s"}}}},
      /*disabled_features=*/{});

  base::Time start_time = base::Time::Now();
  EpochTopics epoch_topics = CreateTestEpochTopics(start_time);

  bool expiration_callback_invoked = false;
  base::OnceClosure expiration_callback =
      base::BindLambdaForTesting([&]() { expiration_callback_invoked = true; });

  // Schedule expiration 1 second after the calculation time.
  task_environment_.FastForwardBy(base::Seconds(1));
  epoch_topics.ScheduleExpiration(std::move(expiration_callback));

  // Verify the callback isn't invoked prematurely.
  task_environment_.FastForwardBy(base::Seconds(26));
  EXPECT_FALSE(expiration_callback_invoked);

  // Verify the callback is invoked at the expected expiration time.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(expiration_callback_invoked);
}

}  // namespace browsing_topics
