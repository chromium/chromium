// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_registry.h"

#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

WebsiteSettingsRegistry::~WebsiteSettingsRegistry() = default;

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
    base::Value initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    WebsiteSettingsInfo::LossyStatus lossy_status,
    WebsiteSettingsInfo::ScopingType scoping_type,
    Platforms platform,
    WebsiteSettingsInfo::IncognitoBehavior incognito_behavior) {
#if BUILDFLAG(IS_WIN)
  if (!(platform & PLATFORM_WINDOWS))
    return nullptr;
#elif BUILDFLAG(IS_LINUX)
  if (!(platform & PLATFORM_LINUX))
    return nullptr;
#elif BUILDFLAG(IS_MAC)
  if (!(platform & PLATFORM_MAC))
    return nullptr;
#elif BUILDFLAG(IS_CHROMEOS)
  if (!(platform & PLATFORM_CHROMEOS))
    return nullptr;
#elif BUILDFLAG(IS_ANDROID)
  if (!(platform & PLATFORM_ANDROID))
    return nullptr;
  // Don't sync settings to mobile platforms. The UI is different to desktop and
  // doesn't allow the settings to be managed in the same way. See
  // crbug.com/642184.
  sync_status = WebsiteSettingsInfo::UNSYNCABLE;
#elif BUILDFLAG(IS_IOS)
  if (!(platform & PLATFORM_IOS))
    return nullptr;
  // Don't sync settings to mobile platforms. The UI is different to desktop and
  // doesn't allow the settings to be managed in the same way. See
  // crbug.com/642184.
  sync_status = WebsiteSettingsInfo::UNSYNCABLE;
#elif BUILDFLAG(IS_FUCHSIA)
  if (!(platform & PLATFORM_FUCHSIA))
    return nullptr;
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
  Register(ContentSettingsType::AUTO_SELECT_CERTIFICATE,
           "auto-select-certificate", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::SSL_CERT_DECISIONS, "ssl-cert-decisions",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::APP_BANNER, "app-banner", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID
#if BUILDFLAG(USE_BLINK)
               | PLATFORM_IOS
#endif
           ,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::SITE_ENGAGEMENT, "site-engagement",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID
#if BUILDFLAG(USE_BLINK)
               | PLATFORM_IOS
#endif
           ,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(
      ContentSettingsType::USB_CHOOSER_DATA, "usb-chooser-data", base::Value(),
      WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
      WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE, DESKTOP | PLATFORM_ANDROID,
      WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::IMPORTANT_SITE_INFO, "important-site-info",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
           "permission-autoblocking-data", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::PASSWORD_PROTECTION, "password-protection",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  // Set when an origin is activated for subresource filtering and the
  // associated UI is shown to the user. Cleared when a site is de-activated or
  // the first URL matching the origin is removed from history.
  Register(ContentSettingsType::ADS_DATA, "subresource-filter-data",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           DESKTOP | PLATFORM_ANDROID
#if BUILDFLAG(USE_BLINK)
               | PLATFORM_IOS
#endif
           ,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(
      ContentSettingsType::MEDIA_ENGAGEMENT, "media-engagement", base::Value(),
      WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
      WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
      DESKTOP | PLATFORM_ANDROID, WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::CLIENT_HINTS, "client-hints", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  // Set to keep track of dismissals without user's interaction for intent
  // picker UI.
  Register(ContentSettingsType::INTENT_PICKER_DISPLAY,
           "intent-picker-auto-display", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE, DESKTOP,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::SERIAL_CHOOSER_DATA, "serial-chooser-data",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE, DESKTOP,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::HID_CHOOSER_DATA, "hid-chooser-data",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE, DESKTOP,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(
      ContentSettingsType::BLUETOOTH_CHOOSER_DATA, "bluetooth-chooser-data",
      /*initial_default_value=*/base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
      WebsiteSettingsInfo::NOT_LOSSY,
      WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE, DESKTOP | PLATFORM_ANDROID,
      WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::SAFE_BROWSING_URL_CHECK_DATA,
           "safe-browsing-url-check-data", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::PERMISSION_AUTOREVOCATION_DATA,
           "permission-autorevocation-data", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA,
           "file-system-access-chooser-data", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE, DESKTOP,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
           "file-system-last-picked-directory", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::FEDERATED_IDENTITY_SHARING, "fedcm-share",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::HTTP_ALLOWED, "http-allowed", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::HTTPS_ENFORCED, "https-enforced", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::FORMFILL_METADATA, "formfill-metadata",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::NOTIFICATION_INTERACTIONS,
           "notification-interactions", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::REDUCED_ACCEPT_LANGUAGE,
           "reduced-accept-language", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::NOTIFICATION_PERMISSION_REVIEW,
           "notification-permission-review", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::PRIVATE_NETWORK_CHOOSER_DATA,
           "private-network-chooser-data", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE, DESKTOP,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(
      ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
      "fedcm-idp-signin", base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
      WebsiteSettingsInfo::NOT_LOSSY,
      WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
      WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
           "unused-site-permissions", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(
      ContentSettingsType::FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION,
      "fedcm-idp-registration", base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
      WebsiteSettingsInfo::NOT_LOSSY,
      WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE, ALL_PLATFORMS,
      WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::COOKIE_CONTROLS_METADATA,
           "cookie-controls-metadata", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::REQUESTING_SCHEMEFUL_SITE_ONLY_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::SMART_CARD_DATA, "smart-card-data",
           base::Value(), WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
           // Add more platforms as implementation progresses.
           // Target is DESKTOP.
           PLATFORM_CHROMEOS, WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
  Register(ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
           "abusive-notification-permissions", base::Value(),
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE,
           DESKTOP | PLATFORM_ANDROID,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
}

}  // namespace content_settings
