// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_

#include <memory>

#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

class TestUtils {
 public:
  TestUtils() = delete;
  TestUtils(const TestUtils&) = delete;
  TestUtils& operator=(const TestUtils&) = delete;

  // The following functions return the content setting (represented as
  // Value, PermissionSetting or ContentSetting enum) from |provider| for the
  // given |content_type|. The returned setting applies to the primary and
  // secondary URL, and to the normal or incognito mode, depending on
  // |include_incognito|.
  static base::Value GetContentSettingValue(const ProviderInterface* provider,
                                            const GURL& primary_url,
                                            const GURL& secondary_url,
                                            ContentSettingsType content_type,
                                            bool include_incognito,
                                            RuleMetaData* metadata = nullptr);

  static std::optional<PermissionSetting> GetPermissionSetting(
      const ProviderInterface* provider,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool include_incognito,
      RuleMetaData* metadata = nullptr);

  static ContentSetting GetContentSetting(const ProviderInterface* provider,
                                          const GURL& primary_url,
                                          const GURL& secondary_url,
                                          ContentSettingsType content_type,
                                          bool include_incognito,
                                          RuleMetaData* metadata = nullptr);

  static base::Time GetLastModified(
      const content_settings::ProviderInterface* provider,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType type);

  // Replace a provider with a different instance for testing purposes
  static void OverrideProvider(
      HostContentSettingsMap* map,
      std::unique_ptr<content_settings::ObservableProvider> provider,
      content_settings::ProviderType type);

  // Returns some value that is valid for content_type and different from the
  // default value.
  static base::Value GetSomeValue(ContentSettingsType content_type);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_
