// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_rule.h"

#include <list>
#include <utility>
#include <vector>

#include "components/content_settings/core/common/content_settings_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {

namespace {

class ListIterator : public RuleIterator {
 public:
  explicit ListIterator(std::list<Rule> rules) : rules_(std::move(rules)) {}

  ~ListIterator() override = default;

  bool HasNext() const override { return !rules_.empty(); }

  Rule Next() override {
    EXPECT_FALSE(rules_.empty());
    Rule rule = std::move(rules_.front());
    rules_.pop_front();
    return rule;
  }

 private:
  std::list<Rule> rules_;
};

}  // namespace

TEST(RuleTest, ConcatenationIterator) {
  base::Time expiredTime = base::Time::Now() - base::Seconds(101);
  base::Time validTime = base::Time::Now() - base::Seconds(101);

  std::list<Rule> rules1;
  rules1.push_back(Rule(ContentSettingsPattern::FromString("a"),
                        ContentSettingsPattern::Wildcard(), base::Value(0),
                        {}));
  rules1.push_back(Rule(
      ContentSettingsPattern::FromString("b"),
      ContentSettingsPattern::Wildcard(), base::Value(0),
      {.expiration = expiredTime, .session_model = SessionModel::UserSession}));
  std::list<Rule> rules2;
  rules2.push_back(
      Rule(ContentSettingsPattern::FromString("c"),
           ContentSettingsPattern::Wildcard(), base::Value(0),
           {.expiration = validTime, .session_model = SessionModel::Durable}));
  rules2.push_back(Rule(ContentSettingsPattern::FromString("d"),
                        ContentSettingsPattern::Wildcard(), base::Value(0),
                        {.expiration = base::Time(),
                         .session_model = SessionModel::UserSession}));

  std::vector<std::unique_ptr<RuleIterator>> iterators;
  iterators.push_back(std::make_unique<ListIterator>(std::move(rules1)));
  iterators.push_back(std::make_unique<ListIterator>(std::move(rules2)));
  ConcatenationIterator concatenation_iterator(std::move(iterators), nullptr);

  Rule rule;
  ASSERT_TRUE(concatenation_iterator.HasNext());
  rule = concatenation_iterator.Next();
  EXPECT_EQ(rule.primary_pattern, ContentSettingsPattern::FromString("a"));
  EXPECT_EQ(rule.metadata.expiration, base::Time());
  EXPECT_EQ(rule.metadata.session_model, SessionModel::Durable);

  ASSERT_TRUE(concatenation_iterator.HasNext());
  rule = concatenation_iterator.Next();
  EXPECT_EQ(rule.primary_pattern, ContentSettingsPattern::FromString("b"));
  EXPECT_EQ(rule.metadata.expiration, expiredTime);
  EXPECT_EQ(rule.metadata.session_model, SessionModel::UserSession);

  ASSERT_TRUE(concatenation_iterator.HasNext());
  rule = concatenation_iterator.Next();
  EXPECT_EQ(rule.primary_pattern, ContentSettingsPattern::FromString("c"));
  EXPECT_EQ(rule.metadata.expiration, validTime);
  EXPECT_EQ(rule.metadata.session_model, SessionModel::Durable);

  ASSERT_TRUE(concatenation_iterator.HasNext());
  rule = concatenation_iterator.Next();
  EXPECT_EQ(rule.primary_pattern, ContentSettingsPattern::FromString("d"));
  EXPECT_EQ(rule.metadata.expiration, base::Time());
  EXPECT_EQ(rule.metadata.session_model, SessionModel::UserSession);

  EXPECT_FALSE(concatenation_iterator.HasNext());
}

}  // namespace content_settings
