// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_H_

#include <stddef.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

class PrefService;
class PrefChangeRegistrar;

namespace content_settings {

class RuleIterator;

// Represents a single pref for reading/writing content settings of one type.
class ContentSettingsPref {
 public:
  typedef base::Callback<void(const ContentSettingsPattern&,
                              const ContentSettingsPattern&,
                              ContentSettingsType,
                              const std::string&)> NotifyObserversCallback;

  ContentSettingsPref(ContentSettingsType content_type,
                      PrefService* prefs,
                      PrefChangeRegistrar* registrar,
                      const std::string& pref_name,
                      bool off_the_record,
                      NotifyObserversCallback notify_callback);
  ~ContentSettingsPref();

  // Returns nullptr to indicate the RuleIterator is empty.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      const ResourceIdentifier& resource_identifier,
      bool off_the_record) const;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         const ResourceIdentifier& resource_identifier,
                         base::Time modified_time,
                         std::unique_ptr<base::Value>&& value);

  // Returns the |last_modified| date of a setting.
  base::Time GetWebsiteSettingLastModified(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      const ResourceIdentifier& resource_identifier);

  void ClearPref();

  void ClearAllContentSettingsRules();

  size_t GetNumExceptions();

  // Tries to lock |lock_|. If successful, returns true and releases the lock.
  bool TryLockForTesting() const;
  void set_allow_resource_identifiers_for_testing() {
    allow_resource_identifiers_ = true;
  }
  void reset_allow_resource_identifiers_for_testing() {
    allow_resource_identifiers_ = false;
  }

 private:
  // Reads all content settings exceptions from the preference and loads them
  // into the |value_map_|. The |value_map_| is cleared first.
  void ReadContentSettingsFromPref();

  // Callback for changes in the pref with the same name.
  void OnPrefChanged();

  // Update the preference that stores content settings exceptions and syncs the
  // value to the obsolete preference. When calling this function, |lock_|
  // should not be held, since this function will send out notifications of
  // preference changes.
  void UpdatePref(const ContentSettingsPattern& primary_pattern,
                  const ContentSettingsPattern& secondary_pattern,
                  const ResourceIdentifier& resource_identifier,
                  const base::Time last_modified,
                  const base::Value* value);

  // In the debug mode, asserts that |lock_| is not held by this thread. It's
  // ok if some other thread holds |lock_|, as long as it will eventually
  // release it.
  void AssertLockNotHeld() const;

  // The type of content settings stored in this pref.
  ContentSettingsType content_type_;

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  PrefService* prefs_;

  // Owned by the PrefProvider.
  PrefChangeRegistrar* registrar_;

  // Name of the dictionary preference managed by this class.
  const std::string& pref_name_;

  bool off_the_record_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  OriginIdentifierValueMap value_map_;

  OriginIdentifierValueMap off_the_record_value_map_;

  NotifyObserversCallback notify_callback_;

  // Used around accesses to the value map objects to guarantee thread safety.
  mutable base::Lock lock_;

  base::ThreadChecker thread_checker_;

  // Used for setting preferences with resource identifiers to simmulate legacy
  // prefs that did have resource identifiers set.
  bool allow_resource_identifiers_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsPref);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_H_
