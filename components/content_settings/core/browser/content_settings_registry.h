// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_REGISTRY_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_REGISTRY_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

class WebsiteSettingsRegistry;

// This class stores ContentSettingsInfo objects for each content setting in the
// system and provides access to them. Global instances can be fetched and
// methods called from from any thread because all of its public methods are
// const.
class ContentSettingsRegistry {
 public:
  using Map =
      std::map<ContentSettingsType, std::unique_ptr<ContentSettingsInfo>>;
  using const_iterator = MapValueIterator<typename Map::const_iterator,
                                          const ContentSettingsInfo*>;

  static ContentSettingsRegistry* GetInstance();

  ContentSettingsRegistry(const ContentSettingsRegistry&) = delete;
  ContentSettingsRegistry& operator=(const ContentSettingsRegistry&) = delete;

  // Reset the instance for use inside tests.
  void ResetForTest();

  const ContentSettingsInfo* Get(ContentSettingsType type) const;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  friend class ContentSettingsRegistryTest;
  friend struct base::LazyInstanceTraitsBase<ContentSettingsRegistry>;

  ContentSettingsRegistry();
  ContentSettingsRegistry(WebsiteSettingsRegistry* website_settings_registry);
  ~ContentSettingsRegistry();

  void Init();

  typedef uint32_t Platforms;

  // Register a new content setting. This maps an origin to an ALLOW/ASK/BLOCK
  // value (see the ContentSetting enum).
  void Register(ContentSettingsType type,
                const std::string& name,
                ContentSetting initial_default_value,
                WebsiteSettingsInfo::SyncStatus sync_status,
                const std::vector<std::string>& allowlisted_primary_schemes,
                const std::set<ContentSetting>& valid_settings,
                WebsiteSettingsInfo::ScopingType scoping_type,
                Platforms platforms,
                ContentSettingsInfo::IncognitoBehavior incognito_behavior,
                ContentSettingsInfo::OriginRestriction origin_restriction);

  Map content_settings_info_;
  raw_ptr<WebsiteSettingsRegistry> website_settings_registry_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_REGISTRY_H_
