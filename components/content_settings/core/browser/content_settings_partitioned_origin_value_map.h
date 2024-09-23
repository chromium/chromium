// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PARTITIONED_ORIGIN_VALUE_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PARTITIONED_ORIGIN_VALUE_MAP_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"

class GURL;

namespace base {
class Lock;
class Value;
class Clock;
}  // namespace base

namespace content_settings {

class RuleIterator;

// This is like |OriginValueMap|, but supports partitioning with
// |PartitionKey|.
//
// This class is multi-threaded, with some users calling |GetRuleIterator| off
// of the UI thread.
//
// Interacting with this class generally requires holding |GetLock|, and
// modifying rules while iterating over them is not permitted. Notably, due to
// complexity around ensuring the lock is held while iterating,
// |GetRuleIterator| should only be called while the lock is not held, as the
// Iterator itself will hold the lock until it's destroyed.
class PartitionedOriginValueMap {
 public:
  base::Lock& GetLock() const LOCK_RETURNED(lock_) { return lock_; }

  size_t size() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns an iterator for reading the rules for |content_type| and
  // |partition_key|. It is not allowed to call functions of
  // |PartitionedOriginValueMap| (also |GetRuleIterator|) before the
  // iterator has been destroyed.
  //
  // |lock_| will be acquired and held until the returned RuleIterator is
  // destroyed.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      const PartitionKey& partition_key) const LOCKS_EXCLUDED(lock_);

  std::unique_ptr<Rule> GetRule(const GURL& primary_url,
                                const GURL& secondary_url,
                                ContentSettingsType content_type,
                                const PartitionKey& partition_key) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  PartitionedOriginValueMap();

  PartitionedOriginValueMap(
      const PartitionedOriginValueMap&) = delete;
  PartitionedOriginValueMap& operator=(
      const PartitionedOriginValueMap&) = delete;

  ~PartitionedOriginValueMap();

  // Returns a weak pointer to the value. If the value does not exist, |nullptr|
  // is returned.
  const base::Value* GetValue(const GURL& primary_url,
                              const GURL& secondary_url,
                              ContentSettingsType content_type,
                              const PartitionKey& partition_key) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool SetValue(const ContentSettingsPattern& primary_pattern,
                const ContentSettingsPattern& secondary_pattern,
                ContentSettingsType content_type,
                base::Value value,
                const RuleMetaData& metadata,
                const PartitionKey& partition_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool DeleteValue(const ContentSettingsPattern& primary_pattern,
                   const ContentSettingsPattern& secondary_pattern,
                   ContentSettingsType content_type,
                   const PartitionKey& partition_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Deletes all values for the given |content_type| and |partition_key|.
  void DeleteValues(ContentSettingsType content_type,
                    const PartitionKey& partition_key)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Clears all values.
  void clear() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void SetClockForTesting(const base::Clock* clock);

 private:
  mutable base::Lock lock_;
  std::map<PartitionKey, OriginValueMap> partitions_
      GUARDED_BY(lock_);

  raw_ptr<const base::Clock> clock_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PARTITIONED_ORIGIN_VALUE_MAP_H_
