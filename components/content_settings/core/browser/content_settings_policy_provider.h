// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_POLICY_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_POLICY_PROVIDER_H_

// A content settings provider that takes its settings out of policies.

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_value_map.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

// PolicyProvider that provides managed content-settings.
//
// PartitionKey is ignored by this provider because the content settings should
// apply across partitions.
class PolicyProvider : public ObservableProvider {
 public:
  explicit PolicyProvider(PrefService* prefs);

  PolicyProvider(const PolicyProvider&) = delete;
  PolicyProvider& operator=(const PolicyProvider&) = delete;

  ~PolicyProvider() override;
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // ProviderInterface implementations.
  std::unique_ptr<RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito,
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
  struct PrefsForManagedDefaultMapEntry;

  static const PrefsForManagedDefaultMapEntry kPrefsForManagedDefault[];

  // Reads the policy managed default settings.
  void ReadManagedDefaultSettings();

  // Callback for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  // Reads the policy controlled default settings for a specific content type.
  void UpdateManagedDefaultSetting(const PrefsForManagedDefaultMapEntry& entry);

  void ReadManagedContentSettings(bool overwrite);

  void GetContentSettingsFromPreferences()
      EXCLUSIVE_LOCKS_REQUIRED(value_map_.GetLock());

  void GetAutoSelectCertificateSettingsFromPreferences()
      EXCLUSIVE_LOCKS_REQUIRED(value_map_.GetLock());

  void ReadManagedContentSettingsTypes(ContentSettingsType content_type);

  OriginValueMap value_map_;

  raw_ptr<PrefService> prefs_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_POLICY_PROVIDER_H_
