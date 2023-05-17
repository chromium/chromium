// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/pref_names.h"

#include "build/build_config.h"

namespace prefs {

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
const char kManagedDefaultClipboardSetting[] =
    "profile.managed_default_content_settings.clipboard";
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
const char kManagedDefaultPopupsSetting[] =
    "profile.managed_default_content_settings.popups";
const char kManagedDefaultSensorsSetting[] =
    "profile.managed_default_content_settings.sensors";
const char kManagedDefaultWebBluetoothGuardSetting[] =
    "profile.managed_default_content_settings.web_bluetooth_guard";
const char kManagedDefaultWebUsbGuardSetting[] =
    "profile.managed_default_content_settings.web_usb_guard";
const char kManagedDefaultFileSystemReadGuardSetting[] =
    "profile.managed_default_content_settings.file_system_read_guard";
const char kManagedDefaultFileSystemWriteGuardSetting[] =
    "profile.managed_default_content_settings.file_system_write_guard";
const char kManagedDefaultSerialGuardSetting[] =
    "profile.managed_default_content_settings.serial_guard";
const char kManagedDefaultInsecureLocalNetworkSetting[] =
    "profile.managed_default_content_settings.insecure_private_network";
const char kManagedDefaultJavaScriptJitSetting[] =
    "profile.managed_default_content_settings.javascript_jit";
const char kManagedDefaultWebHidGuardSetting[] =
    "profile.managed_default_content_settings.web_hid_guard";
const char kManagedDefaultWindowManagementSetting[] =
    "profile.managed_default_content_settings.window_management";
const char kManagedDefaultLocalFontsSetting[] =
    "profile.managed_default_content_settings.local_fonts";
const char kManagedDefaultThirdPartyStoragePartitioningSetting[] =
    "profile.managed_default_content_settings.third_party_storage_partitioning";

// Preferences that are exclusively used to store managed
// content settings patterns.
const char kManagedClipboardAllowedForUrls[] =
    "profile.managed_clipboard_allowed_for_urls";
const char kManagedClipboardBlockedForUrls[] =
    "profile.managed_clipboard_blocked_for_urls";
const char kManagedAutoSelectCertificateForUrls[] =
    "profile.managed_auto_select_certificate_for_urls";
const char kManagedCookiesAllowedForUrls[] =
    "profile.managed_cookies_allowed_for_urls";
const char kManagedCookiesBlockedForUrls[] =
    "profile.managed_cookies_blocked_for_urls";
const char kManagedCookiesSessionOnlyForUrls[] =
    "profile.managed_cookies_sessiononly_for_urls";
const char kManagedGetDisplayMediaSetSelectAllScreensAllowedForUrls[] =
    "profile.managed_get_display_media_set_select_all_screens_allowed_for_urls";
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
const char kManagedPopupsAllowedForUrls[] =
    "profile.managed_popups_allowed_for_urls";
const char kManagedPopupsBlockedForUrls[] =
    "profile.managed_popups_blocked_for_urls";
const char kManagedSensorsAllowedForUrls[] =
    "profile.managed_sensors_allowed_for_urls";
const char kManagedSensorsBlockedForUrls[] =
    "profile.managed_sensors_blocked_for_urls";
const char kManagedWebUsbAllowDevicesForUrls[] =
    "profile.managed_web_usb_allow_devices_for_urls";
const char kManagedWebUsbAskForUrls[] = "profile.managed_web_usb_ask_for_urls";
const char kManagedWebUsbBlockedForUrls[] =
    "profile.managed_web_usb_blocked_for_urls";
const char kManagedFileSystemReadAskForUrls[] =
    "profile.managed_file_system_read_ask_for_urls";
const char kManagedFileSystemReadBlockedForUrls[] =
    "profile.managed_file_system_read_blocked_for_urls";
const char kManagedFileSystemWriteAskForUrls[] =
    "profile.managed_file_system_write_ask_for_urls";
const char kManagedFileSystemWriteBlockedForUrls[] =
    "profile.managed_file_system_write_blocked_for_urls";
const char kManagedLegacyCookieAccessAllowedForDomains[] =
    "profile.managed_legacy_cookie_access_allowed_for_domains";
const char kManagedSerialAskForUrls[] = "profile.managed_serial_ask_for_urls";
const char kManagedSerialBlockedForUrls[] =
    "profile.managed_serial_blocked_for_urls";
const char kManagedInsecureLocalNetworkAllowedForUrls[] =
    "profile.managed_insecure_private_network_allowed_for_urls";
const char kManagedJavaScriptJitAllowedForSites[] =
    "profile.managed_javascript_jit_allowed_for_sites";
const char kManagedJavaScriptJitBlockedForSites[] =
    "profile.managed_javascript_jit_blocked_for_sites";
const char kManagedWebHidAskForUrls[] = "profile.managed_web_hid_ask_for_urls";
const char kManagedWebHidBlockedForUrls[] =
    "profile.managed_web_hid_blocked_for_urls";
const char kManagedWindowManagementAllowedForUrls[] =
    "profile.managed_window_management_allowed_for_urls";
const char kManagedWindowManagementBlockedForUrls[] =
    "profile.managed_window_management_blocked_for_urls";
const char kManagedLocalFontsAllowedForUrls[] =
    "profile.managed_local_fonts_allowed_for_urls";
const char kManagedLocalFontsBlockedForUrls[] =
    "profile.managed_local_fonts_blocked_for_urls";
const char kManagedThirdPartyStoragePartitioningBlockedForOrigins[] =
    "profile.managed_third_party_storage_partitioning_blocked_for_origins";

// Boolean indicating whether the quiet UI is enabled for notification
// permission requests.
const char kEnableQuietNotificationPermissionUi[] =
    "profile.content_settings.enable_quiet_permission_ui.notifications";

// Enum indicating by which method the quiet UI has been enabled for
// notification permission requests. This is stored as of M88 and will be
// backfilled if the quiet UI is enabled but this preference has no value.
const char kQuietNotificationPermissionUiEnablingMethod[] =
    "profile.content_settings.enable_quiet_permission_ui_enabling_method."
    "notifications";

// Time value indicating when the quiet notification UI was last disabled by the
// user. Only permission action history after this point is taken into account
// for adaptive quiet UI activation.
const char kQuietNotificationPermissionUiDisabledTime[] =
    "profile.content_settings.disable_quiet_permission_ui_time.notifications";

// Boolean that indicates whether the user has ever opened any of the in-context
// cookie controls, i.e. the Page Info cookies subpage, or ChromeGuard.
const char kInContextCookieControlsOpened[] =
    "profile.content_settings.in_content_cookies_controls_opened";

#if BUILDFLAG(IS_ANDROID)
// Enable vibration for web notifications.
const char kNotificationsVibrateEnabled[] = "notifications.vibrate_enabled";
// Peripheral setting for request desktop site. When enabled, we will always
// request desktop site if a keyboard, trackpad, or mouse is attached.
const char kDesktopSitePeripheralSettingEnabled[] =
    "desktop_site.peripheral_setting";
// Display setting for request desktop site. When enabled, we will always
// request desktop site if a monitor is connected.
const char kDesktopSiteDisplaySettingEnabled[] = "desktop_site.display_setting";
#endif

}  // namespace prefs
