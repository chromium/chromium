// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PERMISSION_SETTINGS_REGISTRY_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PERMISSION_SETTINGS_REGISTRY_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

class WebsiteSettingsRegistry;

// This class stores PermissionSettingsInfo objects for each permission setting
// in the system and provides access to them. Global instances can be fetched
// and methods called from from any thread because all of its public methods are
// const.
class PermissionSettingsRegistry {
 public:
  using Map =
      std::map<ContentSettingsType, std::unique_ptr<PermissionSettingsInfo>>;
  using const_iterator = MapValueIterator<typename Map::const_iterator,
                                          const PermissionSettingsInfo*>;

  static PermissionSettingsRegistry* GetInstance();

  PermissionSettingsRegistry(const PermissionSettingsRegistry&) = delete;
  PermissionSettingsRegistry& operator=(const PermissionSettingsRegistry&) =
      delete;

  // Reset the instance for use inside tests.
  void ResetForTesting();

  const PermissionSettingsInfo* Get(ContentSettingsType type) const;

  const_iterator begin() const;
  const_iterator end() const;

  // Register a new content setting. This maps an origin to an ALLOW/ASK/BLOCK
  // value (see the ContentSetting enum).
  const PermissionSettingsInfo* Register(
      ContentSettingsType type,
      const std::string& name,
      PermissionSetting initial_default_value,
      WebsiteSettingsInfo::SyncStatus sync_status,
      const std::vector<std::string>& allowlisted_primary_schemes,
      WebsiteSettingsInfo::ScopingType scoping_type,
      WebsiteSettingsRegistry::Platforms platforms,
      PermissionSettingsInfo::OriginRestriction origin_restriction,
      std::unique_ptr<PermissionSettingsInfo::Delegate> delegate);

 private:
  friend class ContentSettingsRegistryTest;
  friend struct base::LazyInstanceTraitsBase<PermissionSettingsRegistry>;

  PermissionSettingsRegistry();
  explicit PermissionSettingsRegistry(
      WebsiteSettingsRegistry* website_settings_registry);
  ~PermissionSettingsRegistry();

  void Init();

  Map permission_settings_info_;
  raw_ptr<WebsiteSettingsRegistry> website_settings_registry_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_PERMISSION_SETTINGS_REGISTRY_H_
