// Copyright 2017 The Chromium Authors
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
  DEPRECATED_PPAPI_BROKER,
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

  // Website setting which stores the list of client hints that the origin
  // requested the browser to remember. The browser is expected to send all
  // client hints in the HTTP request headers for every resource requested
  // from that origin.
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

  // Setting for enabling auto-select of all screens for getDisplayMediaSet.
  GET_DISPLAY_MEDIA_SET_SELECT_ALL_SCREENS,

  // Content settings for access to serial ports. The "guard" content setting
  // stores whether to allow sites to ask for permission to access a port. The
  // permissions granted to access particular ports are stored in the "chooser
  // data" website setting.
  SERIAL_GUARD,
  SERIAL_CHOOSER_DATA,

  // Nothing is stored in this setting at present. Please refer to
  // PeriodicBackgroundSyncPermissionContext for details on how this permission
  // is ascertained.
  // This content setting is not registered because it does not require access
  // to any existing providers.
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

  // Legacy SameSite cookie behavior. This disables SameSite=Lax-by-default,
  // SameSite=None requires Secure, and Schemeful Same-Site, forcing the
  // legacy behavior wherein 1) cookies that don't specify SameSite are treated
  // as SameSite=None, 2) SameSite=None cookies are not required to be Secure,
  // and 3) schemeful same-site is not active.
  //
  // This will also be used to revert to legacy behavior when future changes
  // in cookie handling are introduced.
  LEGACY_COOKIE_ACCESS,

  // Content settings which stores whether to allow sites to ask for permission
  // to save changes to an original file selected by the user through the
  // File System Access API.
  FILE_SYSTEM_WRITE_GUARD,

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
  // and directories selected through the File System Access API.
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

  // Content setting for Screen Enumeration and Screen Detail functionality.
  // Permits access to detailed multi-screen information, like size and
  // position. Permits placing fullscreen and windowed content on specific
  // screens. See also: https://w3c.github.io/window-placement
  WINDOW_MANAGEMENT,

  // Stores whether to allow insecure websites to make private network requests.
  // See also: https://wicg.github.io/cors-rfc1918
  // Set through enterprise policies only.
  INSECURE_PRIVATE_NETWORK,

  // Content setting which stores whether or not a site can access low-level
  // locally installed font data using the Local Fonts Access API.
  LOCAL_FONTS,

  // Stores per-origin state for permission auto-revocation (for all permission
  // types).
  PERMISSION_AUTOREVOCATION_DATA,

  // Stores per-origin state of the most recently selected directory for the use
  // by the File System Access API.
  FILE_SYSTEM_LAST_PICKED_DIRECTORY,

  // Controls access to the getDisplayMedia API when {preferCurrentTab: true}
  // is specified.
  // TODO(crbug.com/1150788): Also apply this when getDisplayMedia() is called
  // without specifying {preferCurrentTab: true}.
  // No values are stored for this type, this is solely needed to be able to
  // register the PermissionContext.
  DISPLAY_CAPTURE,

  // Website setting to store permissions metadata granted to paths on the local
  // file system via the File System Access API. |FILE_SYSTEM_WRITE_GUARD| is
  // the corresponding "guard" setting.
  FILE_SYSTEM_ACCESS_CHOOSER_DATA,

  // Stores a grant that allows a relying party to send a request for identity
  // information to specified identity providers, potentially through any
  // anti-tracking measures that would otherwise prevent it. This setting is
  // associated with the relying party's origin.
  FEDERATED_IDENTITY_SHARING,

  // Whether to use the v8 optimized JIT for running JavaScript on the page.
  JAVASCRIPT_JIT,

  // Content setting which stores user decisions to allow loading a site over
  // HTTP. Entries are added by hostname when a user bypasses the HTTPS-First
  // Mode interstitial warning when a site does not support HTTPS. Allowed hosts
  // are exact hostname matches -- subdomains of a host on the allowlist must be
  // separately allowlisted.
  HTTP_ALLOWED,

  // Stores metadata related to form fill, such as e.g. whether user data was
  // autofilled on a specific website.
  FORMFILL_METADATA,

  // Setting to indicate that there is an active federated sign-in session
  // between a specified relying party and a specified identity provider for
  // a specified account. When this is present it allows access to session
  // management capabilities between the sites. This setting is associated
  // with the relying party's origin.
  FEDERATED_IDENTITY_ACTIVE_SESSION,

  // Setting to indicate whether Chrome should automatically apply darkening to
  // web content.
  AUTO_DARK_WEB_CONTENT,

  // Setting to indicate whether Chrome should request the desktop view of a
  // site instead of the mobile one.
  REQUEST_DESKTOP_SITE,

  // Setting to indicate whether browser should allow signing into a website via
  // the browser FedCM API.
  FEDERATED_IDENTITY_API,

  // Stores notification interactions per origin for the past 90 days.
  // Interactions per origin are pre-aggregated over seven-day windows: A
  // notification interaction or display is assigned to the last Monday midnight
  // in local time.
  NOTIFICATION_INTERACTIONS,

  // Website setting which stores the last reduced accept language negotiated
  // for a given origin, to be used on future visits to the origin.
  REDUCED_ACCEPT_LANGUAGE,

  // Website setting which is used for NotificationPermissionReviewService to
  // store origin blocklist from review notification permissions feature.
  NOTIFICATION_PERMISSION_REVIEW,

  // Website setting to store permissions granted to access particular devices
  // in private network.
  PRIVATE_NETWORK_GUARD,
  PRIVATE_NETWORK_CHOOSER_DATA,

  // Website setting which stores whether the browser has observed the user
  // signing into an identity-provider based on observing the IdP-SignIn-Status
  // HTTP header.
  FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,

  // Website setting which is used for UnusedSitePermissionsService to
  // store revoked permissions of unused sites from unused site permissions
  // feature.
  REVOKED_UNUSED_SITE_PERMISSIONS,

  NUM_TYPES,
};

struct ContentSettingsTypeHash {
  size_t operator()(ContentSettingsType type) const {
    return static_cast<size_t>(type);
  }
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_TYPES_H_
