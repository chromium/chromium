// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_rule.h"

#include <list>
#include <utility>
#include <vector>

#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

namespace {

class ListIterator : public RuleIterator {
 public:
  explicit ListIterator(std::list<std::unique_ptr<Rule>> rules)
      : rules_(std::move(rules)) {}

  ~ListIterator() override = default;

  bool HasNext() const override { return !rules_.empty(); }

  std::unique_ptr<Rule> Next() override {
    EXPECT_FALSE(rules_.empty());
    std::unique_ptr<Rule> rule = std::move(rules_.front());
    rules_.pop_front();
    return rule;
  }

 private:
  std::list<std::unique_ptr<Rule>> rules_;
};

}  // namespace

TEST(RuleTest, ConcatenationIterator) {
  base::Time expiredTime = base::Time::Now() - base::Seconds(101);
  base::Time validTime = base::Time::Now() - base::Seconds(101);

  std::list<std::unique_ptr<Rule>> rules1;
  rules1.push_back(std::make_unique<Rule>(
      ContentSettingsPattern::FromString("a"),
      ContentSettingsPattern::Wildcard(), base::Value(0), RuleMetaData{}));
  RuleMetaData metadata;
  metadata.SetExpirationAndLifetime(expiredTime, base::Seconds(60));
  metadata.set_session_model(mojom::SessionModel::USER_SESSION);
  rules1.push_back(std::make_unique<Rule>(
      ContentSettingsPattern::FromString("b"),
      ContentSettingsPattern::Wildcard(), base::Value(0), metadata));
  std::list<std::unique_ptr<Rule>> rules2;
  metadata.SetExpirationAndLifetime(validTime, base::Seconds(60));
  metadata.set_session_model(mojom::SessionModel::DURABLE);
  rules2.push_back(std::make_unique<Rule>(
      ContentSettingsPattern::FromString("c"),
      ContentSettingsPattern::Wildcard(), base::Value(0), metadata));
  metadata.SetExpirationAndLifetime(base::Time(), base::TimeDelta());
  metadata.set_session_model(mojom::SessionModel::USER_SESSION);
  rules2.push_back(std::make_unique<Rule>(
      ContentSettingsPattern::FromString("d"),
      ContentSettingsPattern::Wildcard(), base::Value(0), metadata));

  std::vector<std::unique_ptr<RuleIterator>> iterators;
  iterators.push_back(std::make_unique<ListIterator>(std::move(rules1)));
  iterators.push_back(std::make_unique<ListIterator>(std::move(rules2)));
  ConcatenationIterator concatenation_iterator(std::move(iterators));

  ASSERT_TRUE(concatenation_iterator.HasNext());
  std::unique_ptr<Rule> rule = concatenation_iterator.Next();
  EXPECT_EQ(rule->primary_pattern, ContentSettingsPattern::FromString("a"));
  EXPECT_EQ(rule->metadata.expiration(), base::Time());
  EXPECT_EQ(rule->metadata.session_model(), mojom::SessionModel::DURABLE);

  ASSERT_TRUE(concatenation_iterator.HasNext());
  rule = concatenation_iterator.Next();
  EXPECT_EQ(rule->primary_pattern, ContentSettingsPattern::FromString("b"));
  EXPECT_EQ(rule->metadata.expiration(), expiredTime);
  EXPECT_EQ(rule->metadata.session_model(), mojom::SessionModel::USER_SESSION);

  ASSERT_TRUE(concatenation_iterator.HasNext());
  rule = concatenation_iterator.Next();
  EXPECT_EQ(rule->primary_pattern, ContentSettingsPattern::FromString("c"));
  EXPECT_EQ(rule->metadata.expiration(), validTime);
  EXPECT_EQ(rule->metadata.session_model(), mojom::SessionModel::DURABLE);

  ASSERT_TRUE(concatenation_iterator.HasNext());
  rule = concatenation_iterator.Next();
  EXPECT_EQ(rule->primary_pattern, ContentSettingsPattern::FromString("d"));
  EXPECT_EQ(rule->metadata.expiration(), base::Time());
  EXPECT_EQ(rule->metadata.session_model(), mojom::SessionModel::USER_SESSION);

  EXPECT_FALSE(concatenation_iterator.HasNext());
}

}  // namespace content_settings
