// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/test/content_settings_test_utils.h"

#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
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

}  // namespace content_settings
