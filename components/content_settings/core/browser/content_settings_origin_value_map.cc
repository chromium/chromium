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
                   base::MovableAutoLock&& auto_lock,
                   base::AutoReset<bool> iterating)
      : auto_lock_(std::move(auto_lock)),
        current_rule_(current_rule),
        rule_end_(rule_end),
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
  base::MovableAutoLock auto_lock_;
  Iterator current_rule_;
  Iterator rule_end_;
  base::AutoReset<bool> iterating_;
};

}  // namespace

std::unique_ptr<RuleIterator> OriginValueMap::GetRuleIterator(
    ContentSettingsType content_type) const NO_THREAD_SAFETY_ANALYSIS {
  // We access |entries_| here, so we need to lock |auto_lock| first. The lock
  // must be passed to the |RuleIteratorImpl| in a locked state, so that nobody
  // can access |entries_| after |find()| but before the |RuleIteratorImpl| is
  // created.
  base::MovableAutoLock auto_lock(lock_);
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
}

std::unique_ptr<Rule> OriginValueMap::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  const RuleEntry* result = nullptr;
  auto it = entry_index().find(content_type);
  if (it == entry_index().end()) {
    return nullptr;
  }
  result = it->second.Find(primary_url, secondary_url);
  if (result) {
    return std::make_unique<Rule>(
        result->first.primary_pattern, result->first.secondary_pattern,
        result->second.value.Clone(), result->second.metadata);
  }

  return nullptr;
}

size_t OriginValueMap::size() const {
  size_t size = 0;
  for (const auto& entry : entry_index()) {
    size += entry.second.size();
  }
  return size;
}

std::vector<ContentSettingsType> OriginValueMap::types() const {
  std::vector<ContentSettingsType> result;
  for (const auto& entry : entry_index()) {
    result.push_back(entry.first);
  }
  return result;
}

OriginValueMap::OriginValueMap(const base::Clock* clock) : clock_(clock) {
  DCHECK(clock);
}

OriginValueMap::OriginValueMap()
    : OriginValueMap(base::DefaultClock::GetInstance()) {}

OriginValueMap::~OriginValueMap() = default;

const base::Value* OriginValueMap::GetValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  auto it = entry_index().find(content_type);
  if (it == entry_index().end()) {
    return nullptr;
  }
  auto* result = it->second.Find(primary_url, secondary_url);
  if (result) {
    return &result->second.value;
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

  return get_index(content_type)
      .SetValue(primary_pattern, secondary_pattern, std::move(value), metadata);
}

bool OriginValueMap::DeleteValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  CHECK(!iterating_);
  auto it = entry_index().find(content_type);
  if (it == entry_index().end()) {
    return false;
  }
  bool result = it->second.DeleteValue(primary_pattern, secondary_pattern);
  if (it->second.empty()) {
    entry_index().erase(it);
  }
  return result;
}

void OriginValueMap::DeleteValues(ContentSettingsType content_type) {
  CHECK(!iterating_);
  entry_index().erase(content_type);
}

void OriginValueMap::clear() {
  CHECK(!iterating_);
  // Delete all owned value objects.
  return entry_index().clear();
}

void OriginValueMap::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
  base::AutoLock lock(lock_);
  for (auto& index : entry_index()) {
    index.second.SetClockForTesting(clock);  // IN-TEST
  }
}
}  // namespace content_settings
