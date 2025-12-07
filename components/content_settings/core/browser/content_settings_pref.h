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
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
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
                                       ContentSettingsType)>
      NotifyObserversCallback;

  ContentSettingsPref(ContentSettingsType content_type,
                      PrefService* prefs,
                      PrefChangeRegistrar* registrar,
                      const std::string& pref_name,
                      bool off_the_record,
                      bool restore_session,
                      NotifyObserversCallback notify_callback);

  ContentSettingsPref(const ContentSettingsPref&) = delete;
  ContentSettingsPref& operator=(const ContentSettingsPref&) = delete;

  ~ContentSettingsPref();

  // Returns nullptr to indicate the RuleIterator is empty.
  std::unique_ptr<RuleIterator> GetRuleIterator(bool off_the_record) const;

  std::unique_ptr<Rule> GetRule(const GURL& primary_url,
                                const GURL& secondary_url,
                                bool off_the_record) const;

  void SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         base::Value value,
                         RuleMetaData metadata);

  void ClearAllContentSettingsRules();

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
  // A helper function to read settings from a dictionary.
  void ReadSettingsFromDictionary(
      const base::Value::Dict& all_settings_dictionary,
      prefs::DictionaryValueUpdate* mutable_settings)
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
                  const RuleMetaData& metadata);

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

  const std::string pref_name_;

  bool off_the_record_;

  bool restore_session_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  OriginValueMap value_map_;

  OriginValueMap off_the_record_value_map_;

  NotifyObserversCallback notify_callback_;

  base::ThreadChecker thread_checker_;

  raw_ptr<const base::Clock> clock_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_H_
