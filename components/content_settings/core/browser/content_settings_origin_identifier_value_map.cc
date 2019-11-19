// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"

#include <memory>
#include <tuple>

#include "base/compiler_specific.h"
#include "base/logging.h"
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
  // |RuleIteratorImpl| takes the ownership of |auto_lock|.
  RuleIteratorImpl(
      const OriginIdentifierValueMap::Rules::const_iterator& current_rule,
      const OriginIdentifierValueMap::Rules::const_iterator& rule_end,
      base::AutoLock* auto_lock)
      : current_rule_(current_rule),
        rule_end_(rule_end),
        auto_lock_(auto_lock) {
  }
  ~RuleIteratorImpl() override {}

  bool HasNext() const override { return (current_rule_ != rule_end_); }

  Rule Next() override {
    DCHECK(HasNext());
    Rule to_return(current_rule_->first.primary_pattern,
                   current_rule_->first.secondary_pattern,
                   current_rule_->second.value.Clone());
    ++current_rule_;
    return to_return;
  }

 private:
  OriginIdentifierValueMap::Rules::const_iterator current_rule_;
  OriginIdentifierValueMap::Rules::const_iterator rule_end_;
  std::unique_ptr<base::AutoLock> auto_lock_;
};

}  // namespace

OriginIdentifierValueMap::EntryMapKey::EntryMapKey(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier)
    : content_type(content_type),
      resource_identifier(resource_identifier) {
}

bool OriginIdentifierValueMap::EntryMapKey::operator<(
    const OriginIdentifierValueMap::EntryMapKey& other) const {
  return std::tie(content_type, resource_identifier) <
    std::tie(other.content_type, other.resource_identifier);
}

OriginIdentifierValueMap::PatternPair::PatternPair(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern) {
}

bool OriginIdentifierValueMap::PatternPair::operator<(
    const OriginIdentifierValueMap::PatternPair& other) const {
  // Note that this operator is the other way around than
  // |ContentSettingsPattern::operator<|. It sorts patterns with higher
  // precedence first.
  return std::tie(primary_pattern, secondary_pattern) >
         std::tie(other.primary_pattern, other.secondary_pattern);
}

OriginIdentifierValueMap::ValueEntry::ValueEntry() {}

OriginIdentifierValueMap::ValueEntry::~ValueEntry() {}

std::unique_ptr<RuleIterator> OriginIdentifierValueMap::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Lock* lock) const {
  EntryMapKey key(content_type, resource_identifier);
  // We access |entries_| here, so we need to lock |lock_| first. The lock must
  // be passed to the |RuleIteratorImpl| in a locked state, so that nobody can
  // access |entries_| after |find()| but before the |RuleIteratorImpl| is
  // created.
  std::unique_ptr<base::AutoLock> auto_lock;
  if (lock)
    auto_lock.reset(new base::AutoLock(*lock));
  auto it = entries_.find(key);
  if (it == entries_.end())
    return nullptr;
  return std::unique_ptr<RuleIterator>(new RuleIteratorImpl(
      it->second.begin(), it->second.end(), auto_lock.release()));
}

size_t OriginIdentifierValueMap::size() const {
  size_t size = 0;
  for (const auto& entry : entries_)
    size += entry.second.size();
  return size;
}

OriginIdentifierValueMap::OriginIdentifierValueMap() {}

OriginIdentifierValueMap::~OriginIdentifierValueMap() {}

const base::Value* OriginIdentifierValueMap::GetValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  EntryMapKey key(content_type, resource_identifier);
  auto it = entries_.find(key);
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

base::Time OriginIdentifierValueMap::GetLastModified(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  DCHECK(primary_pattern.IsValid());
  DCHECK(secondary_pattern.IsValid());

  EntryMapKey key(content_type, resource_identifier);
  PatternPair patterns(primary_pattern, secondary_pattern);
  auto it = entries_.find(key);
  if (it == entries_.end())
    return base::Time();
  auto r = it->second.find(patterns);
  if (r == it->second.end())
    return base::Time();
  return r->second.last_modified;
}

void OriginIdentifierValueMap::SetValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Time last_modified,
    base::Value value) {
  DCHECK(primary_pattern.IsValid());
  DCHECK(secondary_pattern.IsValid());
  // TODO(raymes): Remove this after we track down the cause of
  // crbug.com/531548.
  CHECK_NE(ContentSettingsType::DEFAULT, content_type);
  EntryMapKey key(content_type, resource_identifier);
  PatternPair patterns(primary_pattern, secondary_pattern);
  ValueEntry* entry = &entries_[key][patterns];
  entry->value = std::move(value);
  entry->last_modified = last_modified;
}

void OriginIdentifierValueMap::DeleteValue(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) {
  EntryMapKey key(content_type, resource_identifier);
  PatternPair patterns(primary_pattern, secondary_pattern);
  auto it = entries_.find(key);
  if (it == entries_.end())
    return;
  it->second.erase(patterns);
  if (it->second.empty())
    entries_.erase(it);
}

void OriginIdentifierValueMap::DeleteValues(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) {
  EntryMapKey key(content_type, resource_identifier);
  entries_.erase(key);
}

void OriginIdentifierValueMap::clear() {
  // Delete all owned value objects.
  entries_.clear();
}

}  // namespace content_settings
