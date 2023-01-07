// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

class WebsiteSettingsInfo;

class ContentSettingsInfo {
 public:
  enum IncognitoBehavior {
    // Content setting will be inherited from regular to incognito profiles
    // as usual. This should only be used for features that don't allow access
    // to user data e.g. popup blocker or features that are allowed by default.
    INHERIT_IN_INCOGNITO,

    // Content settings can be inherited if the setting is less permissive
    // than the initial default value of the content setting. Example: A setting
    // with an initial value of ASK will be inherited if it is set to BLOCK or
    // ASK but ALLOW will become ASK in incognito mode. This should be used for
    // all settings that allow access to user data, e.g. geolocation.
    INHERIT_IF_LESS_PERMISSIVE
  };

  enum StorageBehavior {
    // The setting is stored and used in future sessions.
    PERSISTENT,
  };

  enum OriginRestriction {
    // This flag indicates content types that only allow exceptions to be set
    // on secure origins.
    EXCEPTIONS_ON_SECURE_ORIGINS_ONLY,
    // This flag indicates content types that allow exceptions to be set on
    // secure and insecure origins.
    EXCEPTIONS_ON_SECURE_AND_INSECURE_ORIGINS,
  };

  // This object does not take ownership of |website_settings_info|.
  ContentSettingsInfo(const WebsiteSettingsInfo* website_settings_info,
                      const std::vector<std::string>& allowlisted_schemes,
                      const std::set<ContentSetting>& valid_settings,
                      IncognitoBehavior incognito_behavior,
                      StorageBehavior storage_behavior,
                      OriginRestriction origin_restriction);

  ContentSettingsInfo(const ContentSettingsInfo&) = delete;
  ContentSettingsInfo& operator=(const ContentSettingsInfo&) = delete;

  ~ContentSettingsInfo();

  const WebsiteSettingsInfo* website_settings_info() const {
    return website_settings_info_;
  }
  const std::vector<std::string>& allowlisted_schemes() const {
    return allowlisted_schemes_;
  }

  // Gets the original default setting for a particular content type.
  ContentSetting GetInitialDefaultSetting() const;

  bool IsSettingValid(ContentSetting setting) const;
  bool IsDefaultSettingValid(ContentSetting setting) const;

  IncognitoBehavior incognito_behavior() const { return incognito_behavior_; }
  StorageBehavior storage_behavior() const { return storage_behavior_; }
  OriginRestriction origin_restriction() const { return origin_restriction_; }

 private:
  raw_ptr<const WebsiteSettingsInfo> website_settings_info_;
  const std::vector<std::string> allowlisted_schemes_;
  const std::set<ContentSetting> valid_settings_;
  const IncognitoBehavior incognito_behavior_;
  const StorageBehavior storage_behavior_;
  const OriginRestriction origin_restriction_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_INFO_H_
