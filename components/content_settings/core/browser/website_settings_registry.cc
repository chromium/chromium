// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_registry.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"

namespace {

base::LazyInstance<content_settings::WebsiteSettingsRegistry>::DestructorAtExit
    g_website_settings_registry_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content_settings {

// static
WebsiteSettingsRegistry* WebsiteSettingsRegistry::GetInstance() {
  return g_website_settings_registry_instance.Pointer();
}

WebsiteSettingsRegistry::WebsiteSettingsRegistry() {
  Init();
}

WebsiteSettingsRegistry::~WebsiteSettingsRegistry() {}

void WebsiteSettingsRegistry::ResetForTest() {
  website_settings_info_.clear();
  Init();
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::Get(
    ContentSettingsType type) const {
  const auto& it = website_settings_info_.find(type);
  if (it != website_settings_info_.end())
    return it->second.get();
  return nullptr;
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::GetByName(
    const std::string& name) const {
  for (const auto& entry : website_settings_info_) {
    if (entry.second->name() == name)
      return entry.second.get();
  }
  return nullptr;
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::Register(
    ContentSettingsType type,
    const std::string& name,
    std::unique_ptr<base::Value> initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    WebsiteSettingsInfo::LossyStatus lossy_status,
    WebsiteSettingsInfo::ScopingType scoping_type,
    Platforms platform,
    WebsiteSettingsInfo::IncognitoBehavior incognito_behavior) {

#if defined(OS_WIN)
  if (!(platform & PLATFORM_WINDOWS))
    return nullptr;
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (!(platform & PLATFORM_LINUX))
    return nullptr;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (!(platform & PLATFORM_MAC))
    return nullptr;
#elif defined(OS_CHROMEOS)
  if (!(platform & PLATFORM_CHROMEOS))
    return nullptr;
#elif defined(OS_ANDROID)
  if (!(platform & PLATFORM_ANDROID))
    return nullptr;
  // Don't sync settings to mobile platforms. The UI is different to desktop and
  // doesn't allow the settings to be managed in the same way. See
  // crbug.com/642184.
  sync_status = WebsiteSettingsInfo::UNSYNCABLE;
#elif defined(OS_IOS)
  if (!(platform & PLATFORM_IOS))
    return nullptr;
  // Don't sync settings to mobile platforms. The UI is different to desktop and
  // doesn't allow the settings to be managed in the same way. See
  // crbug.com/642184.
  sync_status = WebsiteSettingsInfo::UNSYNCABLE;
#elif defined(OS_FUCHSIA)
  if (!(platform & PLATFORM_FUCHSIA))
    return nullptr;
  sync_status = WebsiteSettingsInfo::UNSYNCABLE;
#else
#error "Unsupported platform"
#endif

  WebsiteSettingsInfo* info = new WebsiteSettingsInfo(
      type, name, std::move(initial_default_value), sync_status, lossy_status,
      scoping_type, incognito_behavior);
  website_settings_info_[info->type()] = base::WrapUnique(info);
  return info;
}

WebsiteSettingsRegistry::const_iterator WebsiteSettingsRegistry::begin() const {
  return const_iterator(website_settings_info_.begin());
}

WebsiteSettingsRegistry::const_iterator WebsiteSettingsRegistry::end() const {
  return const_iterator(website_settings_info_.end());
}

void WebsiteSettingsRegistry::Init() {
  // TODO(raymes): This registration code should not have to be in a single
  // location. It should be possible to register a setting from the code
  // associated with it.

  // WARNING: The string names of the permissions passed in below are used to
  // generate preference names and should never be changed!

  // Website settings.
  Register(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
           "auto-select-certificate", nullptr, WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           ALL_PLATFORMS, WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(
      CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions", nullptr,
      WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
      WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
      DESKTOP | PLATFORM_ANDROID, WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_APP_BANNER, "app-banner", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, "site-engagement", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, "usb-chooser-data", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, "important-site-info",
           nullptr, WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA,
           "permission-autoblocking-data", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_PASSWORD_PROTECTION, "password-protection",
           nullptr, WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP, WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  // Set when an origin is activated for subresource filtering and the
  // associated UI is shown to the user. Cleared when a site is de-activated or
  // the first URL matching the origin is removed from history.
  Register(CONTENT_SETTINGS_TYPE_ADS_DATA, "subresource-filter-data", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT, "media-engagement", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_CLIENT_HINTS, "client-hints", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  // To counteract the reduced usability of the Flash permission when it becomes
  // ephemeral, we sync the bit indicating that the Flash permission should be
  // displayed in the page info.
  Register(CONTENT_SETTINGS_TYPE_PLUGINS_DATA, "flash-data", nullptr,
           WebsiteSettingsInfo::SYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           DESKTOP, WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
}

}  // namespace content_settings
