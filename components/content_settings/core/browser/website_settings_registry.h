// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_

#include <map>
#include <memory>
#include <string>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// This class stores WebsiteSettingsInfo objects for each website setting in the
// system and provides access to them. Global instances can be fetched and
// methods called from from any thread because all of its public methods are
// const.
class WebsiteSettingsRegistry {
 public:
  typedef uint32_t Platforms;
  // TODO(lshang): Remove this enum when content settings can be registered from
  // within the component in which they are used. When this is possible then
  // ifdefs can be contained within each component.
  enum Platform : Platforms {
    PLATFORM_WINDOWS = 1 << 0,
    PLATFORM_LINUX = 1 << 1,
    PLATFORM_CHROMEOS = 1 << 2,
    PLATFORM_MAC = 1 << 3,
    PLATFORM_ANDROID = 1 << 4,
    PLATFORM_IOS = 1 << 5,
    PLATFORM_FUCHSIA = 1 << 6,

    // Settings only applied to win, mac, linux, chromeos, and fuchsia.
    DESKTOP = PLATFORM_WINDOWS | PLATFORM_LINUX | PLATFORM_CHROMEOS |
              PLATFORM_MAC | PLATFORM_FUCHSIA,

    // Settings applied to all platforms, including win, mac, linux, chromeos,
    // android, ios, and fuchsia.
    ALL_PLATFORMS =
        DESKTOP | PLATFORM_ANDROID | PLATFORM_IOS | PLATFORM_FUCHSIA,
  };

  using Map =
      std::map<ContentSettingsType, std::unique_ptr<WebsiteSettingsInfo>>;
  using const_iterator = MapValueIterator<typename Map::const_iterator,
                                          const WebsiteSettingsInfo*>;

  static WebsiteSettingsRegistry* GetInstance();

  WebsiteSettingsRegistry(const WebsiteSettingsRegistry&) = delete;
  WebsiteSettingsRegistry& operator=(const WebsiteSettingsRegistry&) = delete;

  // Reset the instance for use inside tests.
  void ResetForTest();

  const WebsiteSettingsInfo* Get(ContentSettingsType type) const;
  const WebsiteSettingsInfo* GetByName(const std::string& name) const;

  // Register a new website setting. This maps an origin to an arbitrary
  // base::Value. Returns a pointer to the registered WebsiteSettingsInfo which
  // is owned by the registry.
  // A nullptr will be returned if registration fails (for example if
  // |platforms| doesn't match the current platform).
  const WebsiteSettingsInfo* Register(
      ContentSettingsType type,
      const std::string& name,
      base::Value initial_default_value,
      WebsiteSettingsInfo::SyncStatus sync_status,
      WebsiteSettingsInfo::LossyStatus lossy_status,
      WebsiteSettingsInfo::ScopingType scoping_type,
      Platforms platforms,
      WebsiteSettingsInfo::IncognitoBehavior incognito_behavior);

  const_iterator begin() const;
  const_iterator end() const;

 private:
  friend class ContentSettingsRegistryTest;
  friend class WebsiteSettingsRegistryTest;
  friend struct base::LazyInstanceTraitsBase<WebsiteSettingsRegistry>;

  WebsiteSettingsRegistry();
  ~WebsiteSettingsRegistry();

  void Init();

  Map website_settings_info_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_REGISTRY_H_
