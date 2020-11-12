// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace content_settings {

class TestUtils {
 public:
  // The following two functions return the content setting (represented as
  // Value or directly the ContentSetting enum) from |provider| for the
  // given |content_type|. The returned content
  // setting applies to the primary and secondary URL, and to the normal or
  // incognito mode, depending on |include_incognito|.
  static base::Value* GetContentSettingValue(
      const ProviderInterface* provider,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool include_incognito);

  static ContentSetting GetContentSetting(
      const ProviderInterface* provider,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool include_incognito);

  // This wrapper exists only to make
  // HostContentSettingsMap::GetContentSettingValueAndPatterns public for use in
  // tests.
  static std::unique_ptr<base::Value> GetContentSettingValueAndPatterns(
      content_settings::RuleIterator* rule_iterator,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsPattern* primary_pattern,
      ContentSettingsPattern* secondary_pattern);

  // Replace a provider with a different instance for testing purposes
  static void OverrideProvider(
      HostContentSettingsMap* map,
      std::unique_ptr<content_settings::ObservableProvider> provider,
      HostContentSettingsMap::ProviderType type);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TestUtils);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_TEST_CONTENT_SETTINGS_TEST_UTILS_H_
