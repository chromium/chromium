// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>

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

  EntryMap::iterator begin() { return entries_.begin(); }

  EntryMap::iterator end() { return entries_.end(); }

  EntryMap::const_iterator begin() const { return entries_.begin(); }

  EntryMap::const_iterator end() const { return entries_.end(); }

  EntryMap::iterator find(ContentSettingsType content_type) {
    return entries_.find(content_type);
  }

  bool empty() const { return size() == 0u; }

  size_t size() const;

  // Returns an iterator for reading the rules for |content_type| and
  // |resource_identifier|. It is not allowed to call functions of
  // |OriginIdentifierValueMap| (also |GetRuleIterator|) before the iterator
  // has been destroyed. If |lock| is non-NULL, the returned |RuleIterator|
  // locks it and releases it when it is destroyed.
  // Returns nullptr to indicate the RuleIterator is empty.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      base::Lock* lock) const;

  OriginIdentifierValueMap();

  OriginIdentifierValueMap(const OriginIdentifierValueMap&) = delete;
  OriginIdentifierValueMap& operator=(const OriginIdentifierValueMap&) = delete;

  ~OriginIdentifierValueMap();

  // Returns a weak pointer to the value for the given |primary_pattern|,
  // |secondary_pattern|, |content_type| tuple. If
  // no value is stored for the passed parameter |NULL| is returned.
  const base::Value* GetValue(const GURL& primary_url,
                              const GURL& secondary_url,
                              ContentSettingsType content_type) const;

  // Sets the |value| for the given |primary_pattern|, |secondary_pattern|,
  // |content_type| tuple. The caller can also store a
  // |last_modified| date for each value. The |constraints| will be used to
  // constrain the setting to a valid time-range and lifetime model if
  // specified.
  void SetValue(const ContentSettingsPattern& primary_pattern,
                const ContentSettingsPattern& secondary_pattern,
                ContentSettingsType content_type,
                base::Value value,
                const RuleMetaData& metadata);

  // Deletes the map entry for the given |primary_pattern|,
  // |secondary_pattern|, |content_type| tuple.
  void DeleteValue(const ContentSettingsPattern& primary_pattern,
                   const ContentSettingsPattern& secondary_pattern,
                   ContentSettingsType content_type);

  // Deletes all map entries for the given |content_type|.
  void DeleteValues(ContentSettingsType content_type);

  // Clears all map entries.
  void clear();

 private:
  EntryMap entries_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_ORIGIN_IDENTIFIER_VALUE_MAP_H_
