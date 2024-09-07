// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_H_

#include <stddef.h>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_partitioned_origin_value_map.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

class PrefService;
class PrefChangeRegistrar;

namespace base {
class Clock;
}

namespace prefs {
class DictionaryValueUpdate;
}  // namespace prefs

namespace content_settings {

class RuleIterator;

// Represents a single pref for reading/writing content settings of one type.
class ContentSettingsPref {
 public:
  typedef base::RepeatingCallback<void(const ContentSettingsPattern&,
                                       const ContentSettingsPattern&,
                                       ContentSettingsType,
                                       const PartitionKey*)>
      NotifyObserversCallback;

  ContentSettingsPref(ContentSettingsType content_type,
                      PrefService* prefs,
                      PrefChangeRegistrar* registrar,
                      const std::string& pref_name,
                      const std::string& partitioned_pref_name,
                      bool off_the_record,
                      bool restore_session,
                      NotifyObserversCallback notify_callback);

  ContentSettingsPref(const ContentSettingsPref&) = delete;
  ContentSettingsPref& operator=(const ContentSettingsPref&) = delete;

  ~ContentSettingsPref();

  // Returns nullptr to indicate the RuleIterator is empty.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      bool off_the_record,
      const PartitionKey& partition_key) const;

  std::unique_ptr<Rule> GetRule(const GURL& primary_url,
                                const GURL& secondary_url,
                                bool off_the_record,
                                const PartitionKey& partition_key) const;

  void SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         base::Value value,
                         const RuleMetaData& metadata,
                         const PartitionKey& partition_key);

  void ClearAllContentSettingsRules(const PartitionKey& partition_key);

  // Resets pointers that should be released in ShutdownOnUIThread().
  void OnShutdown();

  size_t GetNumExceptions();

  // Tries to lock |lock_|. If successful, returns true and releases the lock.
  bool TryLockForTesting() const;

  void SetClockForTesting(const base::Clock* clock);

 private:
  // Reads all content settings exceptions from the preference and loads them
  // into the |value_map_|. The |value_map_| is cleared first.
  void ReadContentSettingsFromPref();
  // A helper function to read the pref for one partition.
  void ReadContentSettingsFromPrefForPartition(
      const PartitionKey& partition_key,
      const base::Value::Dict& partition,
      prefs::DictionaryValueUpdate* mutable_partition)
      EXCLUSIVE_LOCKS_REQUIRED(value_map_.GetLock());
  // Helper function to determine if the setting should be removed.
  bool ShouldRemoveSetting(base::Time expiration,
                           content_settings::mojom::SessionModel session_model);

  // Callback for changes in the pref with the same name.
  void OnPrefChanged();

  // Update the preference that stores content settings exceptions and syncs the
  // value to the obsolete preference. When calling this function, |lock_|
  // should not be held, since this function will send out notifications of
  // preference changes.
  void UpdatePref(const ContentSettingsPattern& primary_pattern,
                  const ContentSettingsPattern& secondary_pattern,
                  base::Value value,
                  const RuleMetaData& metadata,
                  const PartitionKey& partition_key);

  // In the debug mode, asserts that |lock_| is not held by this thread. It's
  // ok if some other thread holds |lock_|, as long as it will eventually
  // release it.
  void AssertLockNotHeld() const;

  // The type of content settings stored in this pref.
  ContentSettingsType content_type_;

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  raw_ptr<PrefService> prefs_;

  // Owned by the PrefProvider.
  raw_ptr<PrefChangeRegistrar> registrar_;

  // For backward compatibility, we store the data in two pref entries.
  //
  // - `pref_name_` will be used to store the exceptions for the default
  //   partition. The value is a dict mapping from pattern pairs to the
  //   settings. So, nothing has changed for this pref entry, and it is backward
  //   compatible.
  // - `partitioned_pref_name_` is a new pref entry, and it will be used to
  //   store the exceptions for all non-default partitions. The value is a dict
  //   mapping from a serialized `PartitionKey` to the data for this partition.
  //   The data for a partition has exactly the same format as `pref_name_`'s
  //   (i.e. a dict mapping from pattern pairs to the settings).
  //
  // Pottentially, we can deprecate `pref_name_` and use
  // `partitioned_pref_name_` to store all the data. But this will require
  // careful migration to avoid issues such as loss of data.
  const std::string pref_name_;
  const std::string partitioned_pref_name_;

  bool off_the_record_;

  bool restore_session_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  PartitionedOriginValueMap value_map_;

  PartitionedOriginValueMap off_the_record_value_map_;

  NotifyObserversCallback notify_callback_;

  base::ThreadChecker thread_checker_;

  raw_ptr<const base::Clock> clock_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_H_
