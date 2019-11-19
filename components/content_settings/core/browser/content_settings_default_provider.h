// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

// Provider that provides default content settings based on
// user prefs. If no default values are set by the user we use the hard coded
// default values.
class DefaultProvider : public ObservableProvider {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  DefaultProvider(PrefService* prefs,
                  bool incognito);
  ~DefaultProvider() override;

  // ProviderInterface implementations.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      bool incognito) const override;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const ResourceIdentifier& resource_identifier,
                         std::unique_ptr<base::Value>&& value) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

  void ShutdownOnUIThread() override;

 private:
  // Reads all settings from the pref service.
  void ReadDefaultSettings();

  // Change the remembered setting in the memory.
  void ChangeSetting(ContentSettingsType content_type, base::Value* value);

  // True if |value| is NULL or it is the default value for |content_type|.
  bool IsValueEmptyOrDefault(ContentSettingsType content_type,
                             base::Value* value);

  // Reads the preference corresponding to |content_type|.
  std::unique_ptr<base::Value> ReadFromPref(ContentSettingsType content_type);

  // Writes the value |value| to the preference corresponding to |content_type|.
  // It's the responsibility of caller to obtain a lock and notify observers.
  void WriteToPref(ContentSettingsType content_type,
                   base::Value* value);

  // Called on prefs change.
  void OnPreferenceChanged(const std::string& pref_name);

  // Clean up the obsolete preferences from the user's profile.
  void DiscardObsoletePreferences();

  // Copies of the pref data, so that we can read it on the IO thread.
  std::map<ContentSettingsType, std::unique_ptr<base::Value>> default_settings_;

  PrefService* prefs_;

  // Whether this settings map is for an Incognito session.
  const bool is_incognito_;

  // Used around accesses to the |default_settings_| object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  PrefChangeRegistrar pref_change_registrar_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  DISALLOW_COPY_AND_ASSIGN(DefaultProvider);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_
