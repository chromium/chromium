// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_util.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/permissions/features.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"

using content::PermissionType;

namespace permissions {

// The returned strings must match any Field Trial configs for the Permissions
// kill switch e.g. Permissions.Action.Geolocation etc..
std::string PermissionUtil::GetPermissionString(
    ContentSettingsType content_type) {
  switch (content_type) {
    case ContentSettingsType::GEOLOCATION:
      return "Geolocation";
    case ContentSettingsType::NOTIFICATIONS:
      return "Notifications";
    case ContentSettingsType::MIDI_SYSEX:
      return "MidiSysEx";
    case ContentSettingsType::DURABLE_STORAGE:
      return "DurableStorage";
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
    case ContentSettingsType::MEDIASTREAM_MIC:
      return "AudioCapture";
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return "VideoCapture";
    case ContentSettingsType::MIDI:
      return "Midi";
    case ContentSettingsType::BACKGROUND_SYNC:
      return "BackgroundSync";
    case ContentSettingsType::SENSORS:
      return "Sensors";
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      return "AccessibilityEvents";
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return "ClipboardReadWrite";
    case ContentSettingsType::CLIPBOARD_SANITIZED_WRITE:
      return "ClipboardSanitizedWrite";
    case ContentSettingsType::PAYMENT_HANDLER:
      return "PaymentHandler";
    case ContentSettingsType::BACKGROUND_FETCH:
      return "BackgroundFetch";
    case ContentSettingsType::IDLE_DETECTION:
      return "IdleDetection";
    case ContentSettingsType::PERIODIC_BACKGROUND_SYNC:
      return "PeriodicBackgroundSync";
    case ContentSettingsType::WAKE_LOCK_SCREEN:
      return "WakeLockScreen";
    case ContentSettingsType::WAKE_LOCK_SYSTEM:
      return "WakeLockSystem";
    case ContentSettingsType::NFC:
      return "NFC";
    case ContentSettingsType::VR:
      return "VR";
    case ContentSettingsType::AR:
      return "AR";
    case ContentSettingsType::STORAGE_ACCESS:
      return "StorageAccess";
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return "CameraPanTiltZoom";
    case ContentSettingsType::WINDOW_PLACEMENT:
      return "WindowPlacement";
    case ContentSettingsType::FONT_ACCESS:
      return "FontAccess";
    case ContentSettingsType::DISPLAY_CAPTURE:
      return "DisplayCapture";
    default:
      break;
  }
  NOTREACHED();
  return std::string();
}

PermissionRequestGestureType PermissionUtil::GetGestureType(bool user_gesture) {
  return user_gesture ? PermissionRequestGestureType::GESTURE
                      : PermissionRequestGestureType::NO_GESTURE;
}

bool PermissionUtil::GetPermissionType(ContentSettingsType type,
                                       PermissionType* out) {
  switch (type) {
    case ContentSettingsType::GEOLOCATION:
      *out = PermissionType::GEOLOCATION;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      *out = PermissionType::NOTIFICATIONS;
      break;
    case ContentSettingsType::MIDI:
      *out = PermissionType::MIDI;
      break;
    case ContentSettingsType::MIDI_SYSEX:
      *out = PermissionType::MIDI_SYSEX;
      break;
    case ContentSettingsType::DURABLE_STORAGE:
      *out = PermissionType::DURABLE_STORAGE;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      *out = PermissionType::VIDEO_CAPTURE;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      *out = PermissionType::AUDIO_CAPTURE;
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      *out = PermissionType::BACKGROUND_SYNC;
      break;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      *out = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
      break;
#endif
    case ContentSettingsType::SENSORS:
      *out = PermissionType::SENSORS;
      break;
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      *out = PermissionType::ACCESSIBILITY_EVENTS;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      *out = PermissionType::CLIPBOARD_READ_WRITE;
      break;
    case ContentSettingsType::PAYMENT_HANDLER:
      *out = PermissionType::PAYMENT_HANDLER;
      break;
    case ContentSettingsType::BACKGROUND_FETCH:
      *out = PermissionType::BACKGROUND_FETCH;
      break;
    case ContentSettingsType::PERIODIC_BACKGROUND_SYNC:
      *out = PermissionType::PERIODIC_BACKGROUND_SYNC;
      break;
    case ContentSettingsType::WAKE_LOCK_SCREEN:
      *out = PermissionType::WAKE_LOCK_SCREEN;
      break;
    case ContentSettingsType::WAKE_LOCK_SYSTEM:
      *out = PermissionType::WAKE_LOCK_SYSTEM;
      break;
    case ContentSettingsType::NFC:
      *out = PermissionType::NFC;
      break;
    case ContentSettingsType::VR:
      *out = PermissionType::VR;
      break;
    case ContentSettingsType::AR:
      *out = PermissionType::AR;
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      *out = PermissionType::STORAGE_ACCESS_GRANT;
      break;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      *out = PermissionType::CAMERA_PAN_TILT_ZOOM;
      break;
    case ContentSettingsType::WINDOW_PLACEMENT:
      *out = PermissionType::WINDOW_PLACEMENT;
      break;
    case ContentSettingsType::FONT_ACCESS:
      *out = PermissionType::FONT_ACCESS;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      *out = PermissionType::IDLE_DETECTION;
      break;
    case ContentSettingsType::DISPLAY_CAPTURE:
      *out = PermissionType::DISPLAY_CAPTURE;
      break;
    default:
      return false;
  }
  return true;
}

bool PermissionUtil::IsPermission(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::GEOLOCATION:
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::MIDI:
    case ContentSettingsType::MIDI_SYSEX:
    case ContentSettingsType::DURABLE_STORAGE:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::BACKGROUND_SYNC:
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
#endif
    case ContentSettingsType::SENSORS:
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
    case ContentSettingsType::PAYMENT_HANDLER:
    case ContentSettingsType::BACKGROUND_FETCH:
    case ContentSettingsType::PERIODIC_BACKGROUND_SYNC:
    case ContentSettingsType::WAKE_LOCK_SCREEN:
    case ContentSettingsType::WAKE_LOCK_SYSTEM:
    case ContentSettingsType::NFC:
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
    case ContentSettingsType::STORAGE_ACCESS:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
    case ContentSettingsType::WINDOW_PLACEMENT:
    case ContentSettingsType::FONT_ACCESS:
    case ContentSettingsType::IDLE_DETECTION:
    case ContentSettingsType::DISPLAY_CAPTURE:
      return true;
    default:
      return false;
  }
}

bool PermissionUtil::IsGuardContentSetting(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::BLUETOOTH_SCANNING:
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
    case ContentSettingsType::HID_GUARD:
      return true;
    default:
      return false;
  }
}

bool PermissionUtil::CanPermissionBeAllowedOnce(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::GEOLOCATION:
      return base::FeatureList::IsEnabled(
          permissions::features::kOneTimeGeolocationPermission);
    default:
      return false;
  }
}

// Returns the last committed URL for `web_contents`. If the frame's URL is
// about:blank, returns GetLastCommittedOrigin.
// Due to dependency issues, this method is duplicated in
// content/browser/permissions/permission_util.cc.
// TODO(crbug.com/698985): Resolve GetLastCommitted[URL|Origin]() usage.
GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return GetLastCommittedOriginAsURL(web_contents->GetMainFrame());
}

GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // If `allow_universal_access_from_file_urls` flag is enabled, a file can
  // introduce discrepancy between GetLastCommittedURL and
  // GetLastCommittedOrigin. In that case GetLastCommittedURL should be used
  // for requesting and verifying permissions.
  if (web_contents->GetOrCreateWebPreferences()
          .allow_universal_access_from_file_urls &&
      render_frame_host->GetLastCommittedOrigin().GetURL().SchemeIsFile()) {
    return render_frame_host->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  }

  return render_frame_host->GetLastCommittedOrigin().GetURL();
}

}  // namespace permissions
