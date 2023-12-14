// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"

#include <memory>
#include <tuple>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

// This iterator is used for iterating the rules for |content_type| and
// |resource_identifier| in the precedence order of the rules.
class RuleIteratorImpl : public RuleIterator {
 public:
  RuleIteratorImpl(
      const OriginIdentifierValueMap::Rules::const_iterator& current_rule,
      const OriginIdentifierValueMap::Rules::const_iterator& rule_end,
      scoped_refptr<RefCountedAutoLock> auto_lock,
      base::AutoReset<bool> iterating)
      : current_rule_(current_rule),
        rule_end_(rule_end),
        auto_lock_(std::move(auto_lock)),
        iterating_(std::move(iterating)) {}
  ~RuleIteratorImpl() override = default;

  bool HasNext() const override { return (current_rule_ != rule_end_); }

  std::unique_ptr<Rule> Next() override {
    DCHECK(HasNext());
    auto to_return = std::make_unique<UnownedRule>(
        current_rule_->first.primary_pattern,
        current_rule_->first.secondary_pattern, &current_rule_->second.value,
        auto_lock_, current_rule_->second.metadata);
    ++current_rule_;
    return to_return;
  }

 private:
  OriginIdentifierValueMap::Rules::const_iterator current_rule_;
  OriginIdentifierValueMap::Rules::const_iterator rule_end_;
  scoped_refptr<RefCountedAutoLock> auto_lock_;
  base::AutoReset<bool> iterating_;
};

}  // namespace

OriginIdentifierValueMap::PatternPair::PatternPair(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern)
    : primary_pattern(primary_pattern), secondary_pattern(secondary_pattern) {}

bool OriginIdentifierValueMap::PatternPair::operator<(
    const OriginIdentifierValueMap::PatternPair& other) const {
  // Note that this operator is the other way around than
  // |ContentSettingsPattern::operator<|. It sorts patterns with higher
  // precedence first.
  return std::tie(primary_pattern, secondary_pattern) >
         std::tie(other.primary_pattern, other.secondary_pattern);
}

OriginIdentifierValueMap::ValueEntry::ValueEntry() = default;

OriginIdentifierValueMap::ValueEntry::~ValueEntry() = default;

std::unique_ptr<RuleIterator> OriginIdentifierValueMap::GetRuleIterator(
    ContentSettingsType content_type) const NO_THREAD_SAFETY_ANALYSIS {
  // We access |entries_| here, so we need to lock |auto_lock| first. The lock
  // must be passed to the |RuleIteratorImpl| in a locked state, so that nobody
  // can access |entries_| after |find()| but before the |RuleIteratorImpl| is
  // created.
  scoped_refptr<RefCountedAutoLock> auto_lock =
      MakeRefCounted<RefCountedAutoLock>(lock_);
  auto it = entries_.find(content_type);
  if (it == entries_.end()) {
    return nullptr;
  }
  CHECK(!iterating_);
  base::AutoReset<bool> iterating(&iterating_, true);
  return std::make_unique<RuleIteratorImpl>(
      it->second.begin(), it->second.end(), std::move(auto_lock),
      std::move(iterating));
}

size_t OriginIdentifierValueMap::size() const {
  size_t size = 0;
  for (const auto& entry : entries_)
    size += entry.second.size();
  return size;
}

OriginIdentifierValueMap::OriginIdentifierValueMap() = default;

OriginIdentifierValueMap::~OriginIdentifierValueMap() = default;

const base::Value* OriginIdentifierValueMap::GetValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  auto it = entries_.find(content_type);
  if (it == entries_.end())
    return nullptr;

  // Iterate the entries in until a match is found. Since the rules are stored
  // in the order of decreasing precedence, the most specific match is found
  // first.
  for (const auto& entry : it->second) {
    if (entry.first.primary_pattern.Matches(primary_url) &&
        entry.first.secondary_pattern.Matches(secondary_url)) {
      return &entry.second.value;
    }
  }
  return nullptr;
}

bool OriginIdentifierValueMap::SetValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value value,
    const RuleMetaData& metadata) {
  CHECK(!iterating_);
  DCHECK(primary_pattern.IsValid());
  DCHECK(secondary_pattern.IsValid());
  // TODO(raymes): Remove this after we track down the cause of
  // crbug.com/531548.
  CHECK_NE(ContentSettingsType::DEFAULT, content_type);
  PatternPair patterns(primary_pattern, secondary_pattern);
  ValueEntry* entry = &entries_[content_type][patterns];
  if (entry->value == value && entry->metadata == metadata) {
    return false;
  }
  entry->value = std::move(value);
  entry->metadata = metadata;
  return true;
}

bool OriginIdentifierValueMap::DeleteValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  CHECK(!iterating_);
  PatternPair patterns(primary_pattern, secondary_pattern);
  auto it = entries_.find(content_type);
  if (it == entries_.end())
    return false;
  it->second.erase(patterns);
  if (it->second.empty())
    entries_.erase(it);
  return true;
}

void OriginIdentifierValueMap::DeleteValues(ContentSettingsType content_type) {
  CHECK(!iterating_);
  entries_.erase(content_type);
}

void OriginIdentifierValueMap::clear() {
  CHECK(!iterating_);
  // Delete all owned value objects.
  entries_.clear();
}

}  // namespace content_settings
