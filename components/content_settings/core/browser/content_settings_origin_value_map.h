// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_VALUE_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_VALUE_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"

class GURL;
class ContentSettingsPattern;

namespace base {
class Clock;
class Lock;
class Value;
}  // namespace base

namespace content_settings {

// Stores and provides access to Content Settings Rules.
//
// This class is multi-threaded, with some users calling |GetRuleIterator| off
// of the UI thread.
//
// Interacting with this class generally requires holding |GetLock|, and
// modifying rules while iterating over them is not permitted. Notably, due to
// complexity around ensuring the lock is held while iterating,
// |GetRuleIterator| should only be called while the lock is not held, as the
// Iterator itself will hold the lock until it's destroyed.
class OriginValueMap {
 public:
  base::Lock& GetLock() const LOCK_RETURNED(lock_) { return lock_; }

  bool empty() const EXCLUSIVE_LOCKS_REQUIRED(lock_) { return size() == 0u; }

  size_t size() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  std::vector<ContentSettingsType> types() const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns an iterator for reading the rules for |content_type|. It is not
  // allowed to call functions of |OriginValueMap| (also
  // |GetRuleIterator|) before the iterator has been destroyed.
  //
  // |lock_| will be acquired and held until the returned RuleIterator is
  // destroyed.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const LOCKS_EXCLUDED(lock_);

  // Returns the matching Rule with highest precedence or nullptr if no Rule
  // matched.
  std::unique_ptr<Rule> GetRule(const GURL& primary_url,
                                const GURL& secondary_url,
                                ContentSettingsType content_type) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  OriginValueMap();
  explicit OriginValueMap(const base::Clock* clock);

  OriginValueMap(const OriginValueMap&) = delete;
  OriginValueMap& operator=(const OriginValueMap&) = delete;

  ~OriginValueMap();

  // Returns a weak pointer to the value for the given |primary_pattern|,
  // |secondary_pattern|, |content_type| tuple. If
  // no value is stored for the passed parameter |NULL| is returned.
  const base::Value* GetValue(const GURL& primary_url,
                              const GURL& secondary_url,
                              ContentSettingsType content_type) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Sets the |value| for the given |primary_pattern|, |secondary_pattern|,
  // |content_type| tuple. The caller can also store a
  // |last_modified| date for each value. The |constraints| will be used to
  // constrain the setting to a valid time-range and lifetime model if
  // specified.
  // Returns true if something changed.
  bool SetValue(const ContentSettingsPattern& primary_pattern,
                const ContentSettingsPattern& secondary_pattern,
                ContentSettingsType content_type,
                base::Value value,
                const RuleMetaData& metadata) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Deletes the map entry for the given |primary_pattern|,
  // |secondary_pattern|, |content_type| tuple.
  // Returns true if something changed.
  bool DeleteValue(const ContentSettingsPattern& primary_pattern,
                   const ContentSettingsPattern& secondary_pattern,
                   ContentSettingsType content_type)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Deletes all map entries for the given |content_type|.
  void DeleteValues(ContentSettingsType content_type)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Clears all map entries.
  void clear() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void SetClockForTesting(const base::Clock* clock);

 private:
  typedef std::map<ContentSettingsType, HostIndexedContentSettings> EntryIndex;

  EntryIndex& entry_index() EXCLUSIVE_LOCKS_REQUIRED(lock_) { return entries_; }
  const EntryIndex& entry_index() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return entries_;
  }

  HostIndexedContentSettings& get_index(ContentSettingsType type)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    auto [it, is_new] = entry_index().try_emplace(type, clock_);
    return it->second;
  }

  mutable bool iterating_ = false;
  mutable base::Lock lock_;
  EntryIndex entries_ GUARDED_BY(lock_);

  raw_ptr<const base::Clock> clock_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_VALUE_MAP_H_
