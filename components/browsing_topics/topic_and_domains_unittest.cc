// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/topic_and_domains.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_topics {

class TopicAndDomainsTest : public testing::Test {};

TEST_F(TopicAndDomainsTest, FromEmptyDictionaryValue) {
  TopicAndDomains read_topic_and_domains =
      TopicAndDomains::FromDictValue(base::Value::Dict());

  EXPECT_FALSE(read_topic_and_domains.IsValid());
  EXPECT_EQ(read_topic_and_domains.topic(), Topic(0));
  EXPECT_TRUE(read_topic_and_domains.hashed_domains().empty());
}

TEST_F(TopicAndDomainsTest, EmptyTopicAndDomains_ToAndFromDictValue) {
  TopicAndDomains topic_and_domains;

  base::Value::Dict dict_value = topic_and_domains.ToDictValue();
  TopicAndDomains read_topic_and_domains =
      TopicAndDomains::FromDictValue(dict_value);

  EXPECT_FALSE(read_topic_and_domains.IsValid());
  EXPECT_EQ(read_topic_and_domains.topic(), Topic(0));
  EXPECT_TRUE(read_topic_and_domains.hashed_domains().empty());
}

TEST_F(TopicAndDomainsTest, PopulatedTopicAndDomains_ToAndFromValue) {
  TopicAndDomains topic_and_domains(
      Topic(2),
      /*hashed_domains=*/{HashedDomain(123), HashedDomain(456)});

  base::Value::Dict dict_value = topic_and_domains.ToDictValue();
  TopicAndDomains read_topic_and_domains =
      TopicAndDomains::FromDictValue(dict_value);

  EXPECT_TRUE(read_topic_and_domains.IsValid());
  EXPECT_EQ(read_topic_and_domains.topic(), Topic(2));
  EXPECT_EQ(read_topic_and_domains.hashed_domains(),
            std::set({HashedDomain(123), HashedDomain(456)}));
}

TEST_F(TopicAndDomainsTest, ClearDomain) {
  TopicAndDomains topic_and_domains(
      Topic(2),
      /*hashed_domains=*/{HashedDomain(123), HashedDomain(456)});

  topic_and_domains.ClearDomain(HashedDomain(123));

  EXPECT_EQ(topic_and_domains.topic(), Topic(2));
  EXPECT_EQ(topic_and_domains.hashed_domains().size(), 1u);
  EXPECT_EQ(*topic_and_domains.hashed_domains().begin(), HashedDomain(456));
}

}  // namespace browsing_topics
