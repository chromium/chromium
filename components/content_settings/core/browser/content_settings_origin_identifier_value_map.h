// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"

class GURL;

namespace base {
class Lock;
class Value;
}  // namespace base

namespace content_settings {

class RuleIterator;

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
class OriginIdentifierValueMap {
 public:
  struct PatternPair {
    ContentSettingsPattern primary_pattern;
    ContentSettingsPattern secondary_pattern;
    PatternPair(const ContentSettingsPattern& primary_pattern,
                const ContentSettingsPattern& secondary_pattern);
    bool operator<(const OriginIdentifierValueMap::PatternPair& other) const;
  };

  struct ValueEntry {
    base::Value value;
    RuleMetaData metadata;
    ValueEntry();
    ~ValueEntry();
  };

  typedef std::map<PatternPair, ValueEntry> Rules;
  typedef std::map<ContentSettingsType, Rules> EntryMap;

  base::Lock& GetLock() const LOCK_RETURNED(lock_) { return lock_; }

  EntryMap::iterator begin() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return entries_.begin();
  }

  EntryMap::iterator end() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return entries_.end();
  }

  EntryMap::const_iterator begin() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return entries_.begin();
  }

  EntryMap::const_iterator end() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return entries_.end();
  }

  EntryMap::iterator find(ContentSettingsType content_type)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return entries_.find(content_type);
  }

  bool empty() const EXCLUSIVE_LOCKS_REQUIRED(lock_) { return size() == 0u; }

  size_t size() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns an iterator for reading the rules for |content_type| and
  // |resource_identifier|. It is not allowed to call functions of
  // |OriginIdentifierValueMap| (also |GetRuleIterator|) before the iterator
  // has been destroyed.
  //
  // |lock_| will be acquired and held until the returned RuleIterator is
  // destroyed.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type) const LOCKS_EXCLUDED(lock_);

  OriginIdentifierValueMap();

  OriginIdentifierValueMap(const OriginIdentifierValueMap&) = delete;
  OriginIdentifierValueMap& operator=(const OriginIdentifierValueMap&) = delete;

  ~OriginIdentifierValueMap();

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

 private:
  mutable bool iterating_ = false;
  mutable base::Lock lock_;
  EntryMap entries_ GUARDED_BY(lock_);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_
