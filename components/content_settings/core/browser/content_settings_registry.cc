// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_registry.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

namespace {

base::LazyInstance<ContentSettingsRegistry>::DestructorAtExit
    g_content_settings_registry_instance = LAZY_INSTANCE_INITIALIZER;

// TODO(raymes): These overloaded functions make the registration code clearer.
// When initializer lists are available they won't be needed. The initializer
// list can be implicitly or explicitly converted to a std::vector.
std::vector<std::string> WhitelistedSchemes() {
  return std::vector<std::string>();
}

std::vector<std::string> WhitelistedSchemes(const char* scheme1,
                                            const char* scheme2) {
  const char* schemes[] = {scheme1, scheme2};
  return std::vector<std::string>(schemes, schemes + arraysize(schemes));
}

std::vector<std::string> WhitelistedSchemes(const char* scheme1,
                                            const char* scheme2,
                                            const char* scheme3) {
  const char* schemes[] = {scheme1, scheme2, scheme3};
  return std::vector<std::string>(schemes, schemes + arraysize(schemes));
}

std::set<ContentSetting> ValidSettings() {
  return std::set<ContentSetting>();
}

std::set<ContentSetting> ValidSettings(ContentSetting setting1,
                                       ContentSetting setting2) {
  ContentSetting settings[] = {setting1, setting2};
  return std::set<ContentSetting>(settings, settings + arraysize(settings));
}

std::set<ContentSetting> ValidSettings(ContentSetting setting1,
                                       ContentSetting setting2,
                                       ContentSetting setting3) {
  ContentSetting settings[] = {setting1, setting2, setting3};
  return std::set<ContentSetting>(settings, settings + arraysize(settings));
}

std::set<ContentSetting> ValidSettings(ContentSetting setting1,
                                       ContentSetting setting2,
                                       ContentSetting setting3,
                                       ContentSetting setting4) {
  ContentSetting settings[] = {setting1, setting2, setting3, setting4};
  return std::set<ContentSetting>(settings, settings + arraysize(settings));
}

}  // namespace

// static
ContentSettingsRegistry* ContentSettingsRegistry::GetInstance() {
  return g_content_settings_registry_instance.Pointer();
}

ContentSettingsRegistry::ContentSettingsRegistry()
    : ContentSettingsRegistry(WebsiteSettingsRegistry::GetInstance()) {}

ContentSettingsRegistry::ContentSettingsRegistry(
    WebsiteSettingsRegistry* website_settings_registry)
    // This object depends on WebsiteSettingsRegistry, so get it first so that
    // they will be destroyed in reverse order.
    : website_settings_registry_(website_settings_registry) {
  Init();
}

void ContentSettingsRegistry::ResetForTest() {
  website_settings_registry_->ResetForTest();
  content_settings_info_.clear();
  Init();
}

ContentSettingsRegistry::~ContentSettingsRegistry() {}

const ContentSettingsInfo* ContentSettingsRegistry::Get(
    ContentSettingsType type) const {
  const auto& it = content_settings_info_.find(type);
  if (it != content_settings_info_.end())
    return it->second.get();
  return nullptr;
}

ContentSettingsRegistry::const_iterator ContentSettingsRegistry::begin() const {
  return const_iterator(content_settings_info_.begin());
}

ContentSettingsRegistry::const_iterator ContentSettingsRegistry::end() const {
  return const_iterator(content_settings_info_.end());
}

void ContentSettingsRegistry::Init() {
  // TODO(raymes): This registration code should not have to be in a single
  // location. It should be possible to register a setting from the code
  // associated with it.

  // WARNING: The string names of the permissions passed in below are used to
  // generate preference names and should never be changed!

  Register(CONTENT_SETTINGS_TYPE_COOKIES, "cookies", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::SYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_SESSION_ONLY),
           WebsiteSettingsInfo::COOKIES_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_IMAGES, "images", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::SYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme,
                              kExtensionScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::SYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme,
                              kExtensionScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_PLUGINS, "plugins",
           CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
           WebsiteSettingsInfo::SYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK,
                         CONTENT_SETTING_DETECT_IMPORTANT_CONTENT),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::EPHEMERAL);

  Register(CONTENT_SETTINGS_TYPE_POPUPS, "popups", CONTENT_SETTING_BLOCK,
           WebsiteSettingsInfo::SYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme,
                              kExtensionScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::ALL_PLATFORMS,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_GEOLOCATION, "geolocation",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           // See also NotificationPermissionContext::DecidePermission which
           // implements additional incognito exceptions.
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, "automatic-downloads",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE,
           WhitelistedSchemes(kChromeUIScheme, kChromeDevToolsScheme,
                              kExtensionScheme),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  // TODO(raymes): We're temporarily making midi sysex unsyncable while we roll
  // out the kPermissionDelegation feature. We may want to make it syncable
  // again sometime in the future. See https://crbug.com/879954 for details.
  Register(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
           "protected-media-identifier",
// On Android, the default value is ALLOW. See https://crbug.com/854737 for
// details. On ChromeOS the default value is still ASK.
#if defined(OS_ANDROID)
           CONTENT_SETTING_ALLOW,
#else
           CONTENT_SETTING_ASK,
#endif  // defined(OS_ANDROID)
           WebsiteSettingsInfo::UNSYNCABLE, WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE,
           WebsiteSettingsRegistry::PLATFORM_ANDROID |
               WebsiteSettingsRegistry::PLATFORM_CHROMEOS,
#if defined(OS_ANDROID)
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
#else
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
#endif  // defined(OS_ANDROID)
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, "durable-storage",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, "background-sync",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_AUTOPLAY, "autoplay", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_SOUND, "sound", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_ADS, "subresource-filter",
           CONTENT_SETTING_BLOCK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  // Content settings that aren't used to store any data. TODO(raymes): use a
  // different mechanism rather than content settings to represent these.
  // Since nothing is stored in them, there is no real point in them being a
  // content setting.
  Register(CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "protocol-handler",
           CONTENT_SETTING_DEFAULT, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(), ValidSettings(),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, "mixed-script",
           CONTENT_SETTING_DEFAULT, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(), ValidSettings(),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD, "bluetooth-guard",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS, "accessibility-events",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_SENSORS, "sensors", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::UNSYNCABLE, WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_CLIPBOARD_READ, "clipboard",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                         CONTENT_SETTING_ASK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER, "payment-handler",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IN_INCOGNITO,
           ContentSettingsInfo::PERSISTENT);

  Register(CONTENT_SETTINGS_TYPE_USB_GUARD, "usb-guard", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, WhitelistedSchemes(),
           ValidSettings(CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK),
           WebsiteSettingsInfo::SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
           WebsiteSettingsRegistry::DESKTOP |
               WebsiteSettingsRegistry::PLATFORM_ANDROID,
           ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE,
           ContentSettingsInfo::PERSISTENT);
}

void ContentSettingsRegistry::Register(
    ContentSettingsType type,
    const std::string& name,
    ContentSetting initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    const std::vector<std::string>& whitelisted_schemes,
    const std::set<ContentSetting>& valid_settings,
    WebsiteSettingsInfo::ScopingType scoping_type,
    Platforms platforms,
    ContentSettingsInfo::IncognitoBehavior incognito_behavior,
    ContentSettingsInfo::StorageBehavior storage_behavior) {
  // Ensure that nothing has been registered yet for the given type.
  DCHECK(!website_settings_registry_->Get(type));
  std::unique_ptr<base::Value> default_value(
      new base::Value(static_cast<int>(initial_default_value)));
  const WebsiteSettingsInfo* website_settings_info =
      website_settings_registry_->Register(
          type, name, std::move(default_value), sync_status,
          WebsiteSettingsInfo::NOT_LOSSY, scoping_type, platforms,
          WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);

  // WebsiteSettingsInfo::Register() will return nullptr if content setting type
  // is not used on the current platform and doesn't need to be registered.
  if (!website_settings_info)
    return;

  DCHECK(!base::ContainsKey(content_settings_info_, type));
  content_settings_info_[type] = std::make_unique<ContentSettingsInfo>(
      website_settings_info, whitelisted_schemes, valid_settings,
      incognito_behavior, storage_behavior);
}

}  // namespace content_settings
