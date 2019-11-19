// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/pref_names.h"

namespace prefs {

// Boolean that is true if we should unconditionally block third-party cookies,
// regardless of other content settings.
const char kBlockThirdPartyCookies[] = "profile.block_third_party_cookies";

// CookieControlsMode enum value that decides when the cookie controls UI is
// enabled. This will block third-party cookies similar to
// kBlockThirdPartyCookies but with a new UI.
const char kCookieControlsMode[] = "profile.cookie_controls_mode";

// Version of the pattern format used to define content settings.
const char kContentSettingsVersion[] = "profile.content_settings.pref_version";

// Integer that specifies the index of the tab the user was on when they
// last visited the content settings window.
const char kContentSettingsWindowLastTabIndex[] =
    "content_settings_window.last_tab_index";

// Preferences that are exclusively used to store managed values for default
// content settings.
const char kManagedDefaultAdsSetting[] =
    "profile.managed_default_content_settings.ads";
const char kManagedDefaultCookiesSetting[] =
    "profile.managed_default_content_settings.cookies";
const char kManagedDefaultGeolocationSetting[] =
    "profile.managed_default_content_settings.geolocation";
const char kManagedDefaultImagesSetting[] =
    "profile.managed_default_content_settings.images";
const char kManagedDefaultInsecureContentSetting[] =
    "profile.managed_default_content_settings.insecure_content";
const char kManagedDefaultJavaScriptSetting[] =
    "profile.managed_default_content_settings.javascript";
const char kManagedDefaultNotificationsSetting[] =
    "profile.managed_default_content_settings.notifications";
const char kManagedDefaultMediaStreamSetting[] =
    "profile.managed_default_content_settings.media_stream";
const char kManagedDefaultPluginsSetting[] =
    "profile.managed_default_content_settings.plugins";
const char kManagedDefaultPopupsSetting[] =
    "profile.managed_default_content_settings.popups";
const char kManagedDefaultWebBluetoothGuardSetting[] =
    "profile.managed_default_content_settings.web_bluetooth_guard";
const char kManagedDefaultWebUsbGuardSetting[] =
    "profile.managed_default_content_settings.web_usb_guard";
const char kManagedDefaultLegacyCookieAccessSetting[] =
    "profile.managed_default_content_settings.legacy_cookie_access";

// Preferences that are exclusively used to store managed
// content settings patterns.
const char kManagedAutoSelectCertificateForUrls[] =
    "profile.managed_auto_select_certificate_for_urls";
const char kManagedCookiesAllowedForUrls[] =
    "profile.managed_cookies_allowed_for_urls";
const char kManagedCookiesBlockedForUrls[] =
    "profile.managed_cookies_blocked_for_urls";
const char kManagedCookiesSessionOnlyForUrls[] =
    "profile.managed_cookies_sessiononly_for_urls";
const char kManagedImagesAllowedForUrls[] =
    "profile.managed_images_allowed_for_urls";
const char kManagedImagesBlockedForUrls[] =
    "profile.managed_images_blocked_for_urls";
const char kManagedInsecureContentAllowedForUrls[] =
    "profile.managed_insecure_content_allowed_for_urls";
const char kManagedInsecureContentBlockedForUrls[] =
    "profile.managed_insecure_content_blocked_for_urls";
const char kManagedJavaScriptAllowedForUrls[] =
    "profile.managed_javascript_allowed_for_urls";
const char kManagedJavaScriptBlockedForUrls[] =
    "profile.managed_javascript_blocked_for_urls";
const char kManagedNotificationsAllowedForUrls[] =
    "profile.managed_notifications_allowed_for_urls";
const char kManagedNotificationsBlockedForUrls[] =
    "profile.managed_notifications_blocked_for_urls";
const char kManagedPluginsAllowedForUrls[] =
    "profile.managed_plugins_allowed_for_urls";
const char kManagedPluginsBlockedForUrls[] =
    "profile.managed_plugins_blocked_for_urls";
const char kManagedPopupsAllowedForUrls[] =
    "profile.managed_popups_allowed_for_urls";
const char kManagedPopupsBlockedForUrls[] =
    "profile.managed_popups_blocked_for_urls";
const char kManagedWebUsbAllowDevicesForUrls[] =
    "profile.managed_web_usb_allow_devices_for_urls";
const char kManagedWebUsbAskForUrls[] = "profile.managed_web_usb_ask_for_urls";
const char kManagedWebUsbBlockedForUrls[] =
    "profile.managed_web_usb_blocked_for_urls";
const char kManagedLegacyCookieAccessAllowedForDomains[] =
    "profile.managed_legacy_cookie_access_allowed_for_domains";

}  // namespace prefs
