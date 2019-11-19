// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_POLICY_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_POLICY_PROVIDER_H_

// A content settings provider that takes its settings out of policies.

#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

// PolicyProvider that provides managed content-settings.
class PolicyProvider : public ObservableProvider {
 public:
  explicit PolicyProvider(PrefService* prefs);
  ~PolicyProvider() override;
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

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
  struct PrefsForManagedDefaultMapEntry;

  static const PrefsForManagedDefaultMapEntry kPrefsForManagedDefault[];

  // Reads the policy managed default settings.
  void ReadManagedDefaultSettings();

  // Callback for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  // Reads the policy controlled default settings for a specific content type.
  void UpdateManagedDefaultSetting(const PrefsForManagedDefaultMapEntry& entry);

  void ReadManagedContentSettings(bool overwrite);

  void GetContentSettingsFromPreferences(OriginIdentifierValueMap* rules);

  void GetAutoSelectCertificateSettingsFromPreferences(
      OriginIdentifierValueMap* value_map);

  void ReadManagedContentSettingsTypes(ContentSettingsType content_type);

  OriginIdentifierValueMap value_map_;

  PrefService* prefs_;

  PrefChangeRegistrar pref_change_registrar_;

  // Used around accesses to the |value_map_| object to guarantee
  // thread safety.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProvider);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_POLICY_PROVIDER_H_
