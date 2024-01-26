// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
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
                  bool off_the_record,
                  bool should_record_metrics);

  DefaultProvider(const DefaultProvider&) = delete;
  DefaultProvider& operator=(const DefaultProvider&) = delete;

  ~DefaultProvider() override;

  // ProviderInterface implementations.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool off_the_record,
      const PartitionKey& partition_key) const override;

  std::unique_ptr<Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record,
      const PartitionKey& partition_key) const override;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         base::Value&& value,
                         const ContentSettingConstraints& constraints,
                         const PartitionKey& partition_key) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type,
                                    const PartitionKey& partition_key) override;

  void ShutdownOnUIThread() override;

 private:
  // Reads all settings from the pref service.
  void ReadDefaultSettings();

  // Change the remembered setting in the memory. Pass NONE value to reset.
  void ChangeSetting(ContentSettingsType content_type, base::Value value);

  // True if |value| is NONE-type or it is the default value for |content_type|.
  bool IsValueEmptyOrDefault(ContentSettingsType content_type,
                             const base::Value& value);

  // Reads the preference corresponding to |content_type|.
  base::Value ReadFromPref(ContentSettingsType content_type);

  // Writes the value |value| to the preference corresponding to |content_type|.
  // It's the responsibility of caller to obtain a lock and notify observers.
  void WriteToPref(ContentSettingsType content_type, const base::Value& value);

  // Called on prefs change.
  void OnPreferenceChanged(const std::string& pref_name);

  // Clean up the obsolete preferences from the user's profile.
  void DiscardOrMigrateObsoletePreferences();

  // Record Histograms Metrics.
  void RecordHistogramMetrics();

  // Copies of the pref data, so that we can read it on the IO thread.
  std::map<ContentSettingsType, base::Value> default_settings_;

  raw_ptr<PrefService> prefs_;

  // Whether this settings map is for an off-the-record session.
  const bool is_off_the_record_;

  // Used around accesses to the |default_settings_| object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  PrefChangeRegistrar pref_change_registrar_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_DEFAULT_PROVIDER_H_
