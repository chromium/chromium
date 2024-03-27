// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_origin_value_map.h"

#include <memory>
#include <tuple>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

// This iterator is used for iterating the rules for |content_type| and
// |resource_identifier| in the precedence order of the rules.
template <class Iterator>
class RuleIteratorImpl : public RuleIterator {
 public:
  RuleIteratorImpl(const Iterator& current_rule,
                   const Iterator& rule_end,
                   std::unique_ptr<base::AutoLock> auto_lock,
                   base::AutoReset<bool> iterating)
      : current_rule_(current_rule),
        rule_end_(rule_end),
        auto_lock_(std::move(auto_lock)),
        iterating_(std::move(iterating)) {}
  ~RuleIteratorImpl() override = default;

  bool HasNext() const override { return (current_rule_ != rule_end_); }

  std::unique_ptr<Rule> Next() override {
    DCHECK(HasNext());
    auto to_return = std::make_unique<Rule>(
        current_rule_->first.primary_pattern,
        current_rule_->first.secondary_pattern,
        current_rule_->second.value.Clone(), current_rule_->second.metadata);
    ++current_rule_;
    return to_return;
  }

 private:
  Iterator current_rule_;
  Iterator rule_end_;
  std::unique_ptr<base::AutoLock> auto_lock_;
  base::AutoReset<bool> iterating_;
};

}  // namespace

std::unique_ptr<RuleIterator> OriginValueMap::GetRuleIterator(
    ContentSettingsType content_type) const NO_THREAD_SAFETY_ANALYSIS {
  // We access |entries_| here, so we need to lock |auto_lock| first. The lock
  // must be passed to the |RuleIteratorImpl| in a locked state, so that nobody
  // can access |entries_| after |find()| but before the |RuleIteratorImpl| is
  // created.
  auto auto_lock = std::make_unique<base::AutoLock>(lock_);
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    auto it = entry_index().find(content_type);
    if (it == entry_index().end()) {
      return nullptr;
    }
    CHECK(!iterating_);
    base::AutoReset<bool> iterating(&iterating_, true);
    return std::make_unique<
        RuleIteratorImpl<HostIndexedContentSettings::Iterator>>(
        it->second.begin(), it->second.end(), std::move(auto_lock),
        std::move(iterating));
  } else {
    auto it = entry_map().find(content_type);
    if (it == entry_map().end()) {
      return nullptr;
    }
    CHECK(!iterating_);
    base::AutoReset<bool> iterating(&iterating_, true);
    return std::make_unique<RuleIteratorImpl<Rules::const_iterator>>(
        it->second.begin(), it->second.end(), std::move(auto_lock),
        std::move(iterating));
  }
}

std::unique_ptr<Rule> OriginValueMap::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  const RuleEntry* result = nullptr;
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    auto it = entry_index().find(content_type);
    if (it == entry_index().end()) {
      return nullptr;
    }
    result = it->second.Find(primary_url, secondary_url);
  } else {
    auto it = entry_map().find(content_type);
    if (it == entry_map().end()) {
      return nullptr;
    }

    // Iterate the entries in until a match is found. Since the rules are
    // stored in the order of decreasing precedence, the most specific match
    // is found first.
    for (const auto& entry : it->second) {
      if (entry.first.primary_pattern.Matches(primary_url) &&
          entry.first.secondary_pattern.Matches(secondary_url) &&
          (base::FeatureList::IsEnabled(
               content_settings::features::kActiveContentSettingExpiry) ||
           !entry.second.metadata.IsExpired(clock_))) {
        result = &entry;
        break;
      }
    }
  }
  if (result) {
    return std::make_unique<Rule>(
        result->first.primary_pattern, result->first.secondary_pattern,
        result->second.value.Clone(), result->second.metadata);
  }

  return nullptr;
}

size_t OriginValueMap::size() const {
  size_t size = 0;
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    for (const auto& entry : entry_index()) {
      size += entry.second.size();
    }
  } else {
    for (const auto& entry : entry_map()) {
      size += entry.second.size();
    }
  }
  return size;
}

std::vector<ContentSettingsType> OriginValueMap::types() const {
  std::vector<ContentSettingsType> result;
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    for (auto& entry : entry_index()) {
      result.push_back(entry.first);
    }
  } else {
    for (auto& entry : entry_map()) {
      result.push_back(entry.first);
    }
  }
  return result;
}

OriginValueMap::OriginValueMap(base::Clock* clock) : clock_(clock) {
  DCHECK(clock);
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    entries_ = EntryIndex();
  } else {
    entries_ = EntryMap();
  }
}

OriginValueMap::OriginValueMap()
    : OriginValueMap(base::DefaultClock::GetInstance()) {}

OriginValueMap::~OriginValueMap() = default;

const base::Value* OriginValueMap::GetValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    auto it = entry_index().find(content_type);
    if (it == entry_index().end()) {
      return nullptr;
    }
    auto* result = it->second.Find(primary_url, secondary_url);
    if (result) {
      return &result->second.value;
    }
    return nullptr;
  } else {
    auto it = entry_map().find(content_type);
    if (it == entry_map().end()) {
      return nullptr;
    }

    // Iterate the entries in until a match is found. Since the rules are
    // stored in the order of decreasing precedence, the most specific match
    // is found first.
    for (const auto& entry : it->second) {
      if (entry.first.primary_pattern.Matches(primary_url) &&
          entry.first.secondary_pattern.Matches(secondary_url)) {
        return &entry.second.value;
      }
    }
  }
  return nullptr;
}

bool OriginValueMap::SetValue(
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

  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    return get_index(content_type)
        .SetValue(primary_pattern, secondary_pattern, std::move(value),
                  metadata);
  } else {
    SortedPatternPair patterns(primary_pattern, secondary_pattern);
    ValueEntry* entry = &entry_map()[content_type][patterns];
    if (entry->value == value && entry->metadata == metadata) {
      return false;
    }
    entry->value = std::move(value);
    entry->metadata = metadata;
    return true;
  }
}

bool OriginValueMap::DeleteValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  CHECK(!iterating_);
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    auto it = entry_index().find(content_type);
    if (it == entry_index().end()) {
      return false;
    }
    bool result = it->second.DeleteValue(primary_pattern, secondary_pattern);
    if (it->second.empty()) {
      entry_index().erase(it);
    }
    return result;
  } else {
    SortedPatternPair patterns(primary_pattern, secondary_pattern);
    auto it = entry_map().find(content_type);
    if (it == entry_map().end()) {
      return false;
    }
    bool result = it->second.erase(patterns) > 0;
    if (it->second.empty()) {
      entry_map().erase(it);
    }
    return result;
  }
}

void OriginValueMap::DeleteValues(ContentSettingsType content_type) {
  CHECK(!iterating_);
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    entry_index().erase(content_type);
  } else {
    entry_map().erase(content_type);
  }
}

void OriginValueMap::clear() {
  CHECK(!iterating_);
  // Delete all owned value objects.
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    return entry_index().clear();
  } else {
    entry_map().clear();
  }
}

void OriginValueMap::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
  if (base::FeatureList::IsEnabled(features::kIndexedHostContentSettingsMap)) {
    base::AutoLock lock(lock_);
    for (auto& index : entry_index()) {
      index.second.SetClockForTesting(clock);  // IN-TEST
    }
  }
}
}  // namespace content_settings
