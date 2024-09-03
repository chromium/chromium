// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_partitioned_origin_value_map.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

// This is a wrapper wrapping `OriginValueMap`'s `RuleIterator`.
// We need this so that we can attach our own lock.
class RuleIteratorWrapper : public RuleIterator {
 public:
  RuleIteratorWrapper(std::unique_ptr<RuleIterator> rule_iterator_impl,
                      base::MovableAutoLock auto_lock)
      : rule_iterator_impl_(std::move(rule_iterator_impl)),
        auto_lock_(std::move(auto_lock)) {}

  bool HasNext() const override { return rule_iterator_impl_->HasNext(); }

  std::unique_ptr<Rule> Next() override { return rule_iterator_impl_->Next(); }

 private:
  std::unique_ptr<RuleIterator> rule_iterator_impl_;
  base::MovableAutoLock auto_lock_;
};

}  // namespace

std::unique_ptr<RuleIterator>
PartitionedOriginValueMap::GetRuleIterator(
    ContentSettingsType content_type,
    const PartitionKey& partition_key) const NO_THREAD_SAFETY_ANALYSIS {
  base::MovableAutoLock auto_lock(lock_);
  auto it = partitions_.find(partition_key);
  if (it == partitions_.end()) {
    return nullptr;
  }
  auto rule_iterator = it->second.GetRuleIterator(content_type);
  if (rule_iterator == nullptr) {
    return nullptr;
  }
  return std::make_unique<RuleIteratorWrapper>(std::move(rule_iterator),
                                               std::move(auto_lock));
}

std::unique_ptr<Rule> PartitionedOriginValueMap::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const PartitionKey& partition_key) const {
  auto it = partitions_.find(partition_key);
  if (it == partitions_.end()) {
    return nullptr;
  }

  base::AutoLock auto_lock(it->second.GetLock());
  return it->second.GetRule(primary_url, secondary_url, content_type);
}

size_t PartitionedOriginValueMap::size() const {
  size_t size = 0;
  for (const auto& [key, partition] : partitions_) {
    base::AutoLock auto_lock(partition.GetLock());
    size += partition.size();
  }
  return size;
}

PartitionedOriginValueMap::PartitionedOriginValueMap()
    : clock_(base::DefaultClock::GetInstance()) {}

PartitionedOriginValueMap::~PartitionedOriginValueMap() =
    default;

const base::Value* PartitionedOriginValueMap::GetValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const PartitionKey& partition_key) const {
  auto it = partitions_.find(partition_key);
  if (it == partitions_.end()) {
    return nullptr;
  }
  // This function requires the caller to hold the root lock (i.e.
  // `this->lock_`), so it is ok to auto release the "child" lock before we
  // return.
  base::AutoLock auto_lock(it->second.GetLock());
  return it->second.GetValue(primary_url, secondary_url, content_type);
}

bool PartitionedOriginValueMap::SetValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value value,
    const RuleMetaData& metadata,
    const PartitionKey& partition_key) {
  auto [it, is_new] = partitions_.try_emplace(partition_key, clock_);
  base::AutoLock auto_lock(it->second.GetLock());
  return it->second.SetValue(primary_pattern, secondary_pattern, content_type,
                             std::move(value), metadata);
}

bool PartitionedOriginValueMap::DeleteValue(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const PartitionKey& partition_key) {
  auto it = partitions_.find(partition_key);
  if (it == partitions_.end()) {
    return false;
  }
  bool is_empty = false;
  bool updated = false;
  {
    base::AutoLock auto_lock(it->second.GetLock());
    updated = it->second.DeleteValue(primary_pattern, secondary_pattern,
                                     content_type);
    is_empty = it->second.empty();
  }
  if (is_empty) {
    partitions_.erase(it);
  }

  return updated;
}

void PartitionedOriginValueMap::DeleteValues(
    ContentSettingsType content_type,
    const PartitionKey& partition_key) {
  auto it = partitions_.find(partition_key);
  if (it == partitions_.end()) {
    return;
  }
  bool is_empty;
  {
    base::AutoLock auto_lock(it->second.GetLock());
    it->second.DeleteValues(content_type);
    is_empty = it->second.empty();
  }
  if (is_empty) {
    partitions_.erase(it);
  }
}

void PartitionedOriginValueMap::clear() {
  partitions_.clear();
}

void PartitionedOriginValueMap::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
  base::AutoLock lock(lock_);
  for (auto& partition : partitions_) {
    partition.second.SetClockForTesting(clock);  // IN-TEST
  }
}

}  // namespace content_settings
