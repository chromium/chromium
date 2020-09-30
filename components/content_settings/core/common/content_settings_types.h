// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_

#include <stddef.h>
#include <stdint.h>

// A particular type of content to care about.  We give the user various types
// of controls over each of these.
// When adding/removing values from this enum, be sure to update the
// kHistogramValue array in content_settings.cc as well.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.content_settings
enum class ContentSettingsType : int32_t {
  // "DEFAULT" is only used as an argument to the Content Settings Window
  // opener; there it means "whatever was last shown".
  DEFAULT = -1,
  COOKIES = 0,
  IMAGES,
  JAVASCRIPT,
  PLUGINS,

  // This setting governs both popups and unwanted redirects like tab-unders and
  // framebusting.
  // TODO(csharrison): Consider renaming it to POPUPS_AND_REDIRECTS, but it
  // might not be worth the trouble.
  POPUPS,

  GEOLOCATION,
  NOTIFICATIONS,
  AUTO_SELECT_CERTIFICATE,
  MIXEDSCRIPT,
  MEDIASTREAM_MIC,
  MEDIASTREAM_CAMERA,
  PROTOCOL_HANDLERS,
  PPAPI_BROKER,
  AUTOMATIC_DOWNLOADS,
  MIDI_SYSEX,
  SSL_CERT_DECISIONS,
  PROTECTED_MEDIA_IDENTIFIER,
  APP_BANNER,
  SITE_ENGAGEMENT,
  DURABLE_STORAGE,
  USB_CHOOSER_DATA,
  BLUETOOTH_GUARD,
  BACKGROUND_SYNC,
  AUTOPLAY,
  IMPORTANT_SITE_INFO,
  PERMISSION_AUTOBLOCKER_DATA,
  ADS,

  // Website setting which stores metadata for the subresource filter to aid in
  // decisions for whether or not to show the UI.
  ADS_DATA,

  // This is special-cased in the permissions layer to always allow, and as
  // such doesn't have associated prefs data.
  MIDI,

  // This content setting type is for caching password protection service's
  // verdicts of each origin.
  PASSWORD_PROTECTION,

  // Website setting which stores engagement data for media related to a
  // specific origin.
  MEDIA_ENGAGEMENT,

  // Content setting which stores whether or not the site can play audible
  // sound. This will not block playback but instead the user will not hear it.
  SOUND,

  // Website setting which stores the list of client hints (and the preference
  // expiration time for each of the client hints) that the origin requested
  // the browser to remember. Spec:
  // http://httpwg.org/http-extensions/client-hints.html#accept-ch-lifetime.
  // The setting is stored as a dictionary that includes the mapping from
  // different client hints to their respective expiration times (seconds since
  // epoch). The browser is expected to send all the unexpired client hints in
  // the HTTP request headers for every resource requested from that origin.
  CLIENT_HINTS,

  // Generic Sensor API covering ambient-light-sensor, accelerometer, gyroscope
  // and magnetometer are all mapped to a single content_settings_type.
  // Setting for the Generic Sensor API covering ambient-light-sensor,
  // accelerometer, gyroscope and magnetometer. These are all mapped to a single
  // ContentSettingsType.
  SENSORS,

  // Content setting which stores whether or not the user has granted the site
  // permission to respond to accessibility events, which can be used to
  // provide a custom accessibility experience. Requires explicit user consent
  // because some users may not want sites to know they're using assistive
  // technology.
  ACCESSIBILITY_EVENTS,

  // Used to store whether the user has ever changed the Flash permission for
  // a site.
  PLUGINS_DATA,

  // Used to store whether to allow a website to install a payment handler.
  PAYMENT_HANDLER,

  // Content setting which stores whether to allow sites to ask for permission
  // to access USB devices. If this is allowed specific device permissions are
  // stored under USB_CHOOSER_DATA.
  USB_GUARD,

  // Nothing is stored in this setting at present. Please refer to
  // BackgroundFetchPermissionContext for details on how this permission
  // is ascertained.
  BACKGROUND_FETCH,

  // Website setting which stores the amount of times the user has dismissed
  // intent picker UI without explicitly choosing an option.
  INTENT_PICKER_DISPLAY,

  // Used to store whether to allow a website to detect user active/idle state.
  IDLE_DETECTION,

  // Content settings for access to serial ports. The "guard" content setting
  // stores whether to allow sites to ask for permission to access a port. The
  // permissions granted to access particular ports are stored in the "chooser
  // data" website setting.
  SERIAL_GUARD,
  SERIAL_CHOOSER_DATA,

  // Nothing is stored in this setting at present. Please refer to
  // PeriodicBackgroundSyncPermissionContext for details on how this permission
  // is ascertained.
  PERIODIC_BACKGROUND_SYNC,

  // Content setting which stores whether to allow sites to ask for permission
  // to do Bluetooth scanning.
  BLUETOOTH_SCANNING,

  // Content settings for access to HID devices. The "guard" content setting
  // stores whether to allow sites to ask for permission to access a device. The
  // permissions granted to access particular devices are stored in the "chooser
  // data" website setting.
  HID_GUARD,
  HID_CHOOSER_DATA,

  // Wake Lock API, which has two lock types: screen and system locks.
  // Currently, screen locks do not need any additional permission, and system
  // locks are always denied while the right UI is worked out.
  WAKE_LOCK_SCREEN,
  WAKE_LOCK_SYSTEM,

  // Legacy SameSite cookie behavior. This disables SameSiteByDefaultCookies,
  // CookiesWithoutSameSiteMustBeSecure, and SchemefulSameSite, forcing the
  // legacy behavior wherein cookies that don't specify SameSite are treated as
  // SameSite=None, SameSite=None cookies are not required to be Secure, and
  // schemeful same-site is not active.
  //
  // This will also be used to revert to legacy behavior when future changes
  // in cookie handling are introduced.
  LEGACY_COOKIE_ACCESS,

  // Content settings which stores whether to allow sites to ask for permission
  // to save changes to an original file selected by the user through the
  // File System API.
  FILE_SYSTEM_WRITE_GUARD,

  // Content settings for installed web apps that browsing history may be
  // inferred from e.g. last update check timestamp.
  INSTALLED_WEB_APP_METADATA,

  // Used to store whether to allow a website to exchange data with NFC devices.
  NFC,

  // Website setting to store permissions granted to access particular Bluetooth
  // devices.
  BLUETOOTH_CHOOSER_DATA,

  // Full access to the system clipboard (sanitized read without user gesture,
  // and unsanitized read and write with user gesture).
  // TODO(https://crbug.com/1027225): Move CLIPBOARD_READ_WRITE uses to be
  // ordered in the same order as listed in the enum.
  CLIPBOARD_READ_WRITE,

  // This is special-cased in the permissions layer to always allow, and as
  // such doesn't have associated prefs data.
  // TODO(https://crbug.com/1027225): Move CLIPBOARD_SANITIZED_WRITE uses to be
  // ordered in the same order as listed in the enum.
  CLIPBOARD_SANITIZED_WRITE,

  // This content setting type is for caching safe browsing real time url
  // check's verdicts of each origin.
  SAFE_BROWSING_URL_CHECK_DATA,

  // Used to store whether a site is allowed to request AR or VR sessions with
  // the WebXr Device API.
  VR,
  AR,

  // Content setting which stores whether to allow site to open and read files
  // and directories selected through the File System API.
  FILE_SYSTEM_READ_GUARD,

  // Access to first party storage in a third-party context. Exceptions are
  // scoped to the combination of requesting/top-level origin, and are managed
  // through the Storage Access API. For the time being, this content setting
  // exists in parallel to third-party cookie rules stored in COOKIES.
  // TODO(https://crbug.com/989663): Reconcile the two.
  STORAGE_ACCESS,

  // Content setting which stores whether to allow a site to control camera
  // movements. It does not give access to camera.
  CAMERA_PAN_TILT_ZOOM,

  // Content setting for Screen Enumeration and Window Placement functionality.
  // Permits access to information about the screens, like size and position.
  // Permits creating and placing windows across the set of connected screens.
  WINDOW_PLACEMENT,

  // Stores whether to allow insecure websites to make private network requests.
  // See also: https://wicg.github.io/cors-rfc1918
  // Set through enterprise policies only.
  INSECURE_PRIVATE_NETWORK,

  // Content setting which stores whether or not a site can access low-level
  // locally installed font data using the Font Access API.
  FONT_ACCESS,

  // Stores per-origin state for permission auto-revocation (for all permission
  // types).
  PERMISSION_AUTOREVOCATION_DATA,

  NUM_TYPES,
};

struct ContentSettingsTypeHash {
  size_t operator()(ContentSettingsType type) const {
    return static_cast<size_t>(type);
  }
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_
