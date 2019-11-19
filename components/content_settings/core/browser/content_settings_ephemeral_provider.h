// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_EPHEMERAL_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_EPHEMERAL_PROVIDER_H_

#include <set>

#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"

namespace base {
class Clock;
}

namespace content_settings {

// A user-modifiable content settings provider that doesn't store its settings
// on disk.
class EphemeralProvider : public UserModifiableProvider {
 public:
  EphemeralProvider(bool store_last_modified);
  ~EphemeralProvider() override;

  // UserModifiableProvider implementations.
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
  base::Time GetWebsiteSettingLastModified(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier) override;

  void SetClockForTesting(base::Clock* clock);
  void SetSupportedTypesForTesting(
      std::set<ContentSettingsType>& supported_types) {
    supported_types_ = supported_types;
  }
  size_t GetCountForTesting() { return content_settings_rules_.size(); }

 private:
  bool store_last_modified_;

  OriginIdentifierValueMap content_settings_rules_;

  std::set<ContentSettingsType> supported_types_;

  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(EphemeralProvider);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_EPHEMERAL_PROVIDER_H_
