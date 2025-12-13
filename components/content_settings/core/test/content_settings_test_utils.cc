// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/test/content_settings_test_utils.h"

#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace content_settings {

// static
base::Value TestUtils::GetContentSettingValue(const ProviderInterface* provider,
                                              const GURL& primary_url,
                                              const GURL& secondary_url,
                                              ContentSettingsType content_type,
                                              bool include_incognito,
                                              RuleMetaData* metadata) {
  return HostContentSettingsMap::GetContentSettingValueAndPatterns(
      provider, primary_url, secondary_url, content_type, include_incognito,
      nullptr, nullptr, metadata);
}

// static
std::optional<PermissionSetting> TestUtils::GetPermissionSetting(
    const ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool include_incognito,
    RuleMetaData* metadata) {
  auto* info = PermissionSettingsRegistry::GetInstance()->Get(content_type);
  CHECK(info);
  return info->delegate().FromValue(
      GetContentSettingValue(provider, primary_url, secondary_url, content_type,
                             include_incognito, metadata));
}

// static
ContentSetting TestUtils::GetContentSetting(const ProviderInterface* provider,
                                            const GURL& primary_url,
                                            const GURL& secondary_url,
                                            ContentSettingsType content_type,
                                            bool include_incognito,
                                            RuleMetaData* metadata) {
  return ValueToContentSetting(
      GetContentSettingValue(provider, primary_url, secondary_url, content_type,
                             include_incognito, metadata));
}

// static
base::Time TestUtils::GetLastModified(
    const content_settings::ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type) {
  content_settings::RuleMetaData metadata;
  content_settings::TestUtils::GetContentSetting(
      provider, primary_url, secondary_url, type, false, &metadata);
  return metadata.last_modified();
}

// static
void TestUtils::OverrideProvider(
    HostContentSettingsMap* map,
    std::unique_ptr<content_settings::ObservableProvider> provider,
    content_settings::ProviderType type) {
  if (map->content_settings_providers_[type]) {
    map->content_settings_providers_[type]->ShutdownOnUIThread();
  }
  map->content_settings_providers_[type] = std::move(provider);
}

// static
base::Value TestUtils::GetSomeValue(ContentSettingsType content_type) {
  base::Value some_value;
  auto* content_setting_info =
      ContentSettingsRegistry::GetInstance()->Get(content_type);
  if (content_setting_info) {
    for (auto setting :
         {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK}) {
      if (content_setting_info->IsSettingValid(setting) &&
          setting != content_setting_info->GetInitialDefaultSetting()) {
        return base::Value(setting);
      }
    }
    // Some settings don't have any allowed values.
    return base::Value();
  }
  if (auto* permission_info =
          PermissionSettingsRegistry::GetInstance()->Get(content_type)) {
    if (content_type == mojom::ContentSettingsType::GEOLOCATION_WITH_OPTIONS) {
      // Set valid Geolocation PermissionSetting.
      return permission_info->delegate().ToValue(GeolocationSetting{
          PermissionOption::kAllowed, PermissionOption::kAsk});
    } else {
      NOTREACHED() << content_type;
    }
  }

  // Other website settings allow arbitrary values.
  base::Value::Dict dict;
  dict.Set("foo", 42);
  return base::Value(std::move(dict));
}

}  // namespace content_settings
