// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/request_type.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permissions_client.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/resources/android/theme_resources.h"
#else
#include "components/permissions/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace permissions {

namespace {

#if BUILDFLAG(IS_ANDROID)
int GetIconIdAndroid(RequestType type) {
  switch (type) {
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return IDR_ANDROID_INFOBAR_VR_HEADSET;
    case RequestType::kCameraStream:
      return IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA;
    case RequestType::kClipboard:
      return IDR_ANDROID_INFOBAR_CLIPBOARD;
    case RequestType::kDiskQuota:
      return IDR_ANDROID_INFOBAR_FOLDER;
    case RequestType::kGeolocation:
      return IDR_ANDROID_INFOBAR_GEOLOCATION;
    case RequestType::kHandTracking:
      return IDR_ANDROID_INFOBAR_HAND_TRACKING;
    case RequestType::kIdentityProvider:
      return IDR_ANDROID_INFOBAR_IDENTITY_PROVIDER;
    case RequestType::kIdleDetection:
      return IDR_ANDROID_INFOBAR_IDLE_DETECTION;
    case RequestType::kMicStream:
      return IDR_ANDROID_INFOBAR_MEDIA_STREAM_MIC;
    case RequestType::kMidiSysex:
      return IDR_ANDROID_INFOBAR_MIDI;
    case RequestType::kMultipleDownloads:
      return IDR_ANDROID_INFOBAR_MULTIPLE_DOWNLOADS;
    case RequestType::kNfcDevice:
      return IDR_ANDROID_INFOBAR_NFC;
    case RequestType::kNotifications:
      return IDR_ANDROID_INFOBAR_NOTIFICATIONS;
    case RequestType::kProtectedMediaIdentifier:
      return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      return IDR_ANDROID_STORAGE_ACCESS;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/335848275): Migrate the icons in 2 steps.
// 1 - Copy contents of refresh icons into current non-refresh icons.
// 2 - In a separate change, remove the refresh icons.
const gfx::VectorIcon& GetIconIdDesktop(RequestType type) {
  switch (type) {
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return vector_icons::kVrHeadsetChromeRefreshIcon;
    case RequestType::kCameraPanTiltZoom:
    case RequestType::kCameraStream:
      return vector_icons::kVideocamChromeRefreshIcon;
    case RequestType::kCapturedSurfaceControl:
      return vector_icons::kTouchpadMouseIcon;
    case RequestType::kClipboard:
      return vector_icons::kContentPasteIcon;
    case RequestType::kDiskQuota:
      return vector_icons::kFolderChromeRefreshIcon;
    case RequestType::kGeolocation:
      return vector_icons::kLocationOnChromeRefreshIcon;
    case RequestType::kHandTracking:
      return vector_icons::kHandGestureIcon;
    case RequestType::kIdleDetection:
      return vector_icons::kDevicesIcon;
    case RequestType::kKeyboardLock:
      return vector_icons::kKeyboardLockIcon;
    case RequestType::kLocalFonts:
      return vector_icons::kFontDownloadChromeRefreshIcon;
    case RequestType::kMicStream:
      return vector_icons::kMicChromeRefreshIcon;
    case RequestType::kMidiSysex:
      return vector_icons::kMidiChromeRefreshIcon;
    case RequestType::kMultipleDownloads:
      return vector_icons::kFileDownloadChromeRefreshIcon;
    case RequestType::kNotifications:
      return vector_icons::kNotificationsChromeRefreshIcon;
    case RequestType::kPointerLock:
      return vector_icons::kPointerLockIcon;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      // This icon is provided by ChromePermissionsClient::GetOverrideIconId.
      NOTREACHED_IN_MIGRATION();
      return gfx::kNoneIcon;
#endif
    case RequestType::kRegisterProtocolHandler:
      return vector_icons::kProtocolHandlerIcon;
#if BUILDFLAG(IS_CHROMEOS)
    case RequestType::kSmartCard:
      return vector_icons::kSmartCardReaderIcon;
#endif
    case RequestType::kWebAppInstallation:
      // TODO(crbug.com/333795265): provide a dedicated icon.
      return vector_icons::kTouchpadMouseIcon;
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    case RequestType::kWebPrinting:
      return vector_icons::kPrinterIcon;
#endif
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      return vector_icons::kStorageAccessIcon;
    case RequestType::kWindowManagement:
      return vector_icons::kSelectWindowChromeRefreshIcon;
    case RequestType::kFileSystemAccess:
      return vector_icons::kFolderIcon;
    case RequestType::kIdentityProvider:
      // TODO(crbug.com/40252825): provide a dedicated icon.
      return vector_icons::kFolderIcon;
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::kNoneIcon;
}

const gfx::VectorIcon& GetBlockedIconIdDesktop(RequestType type) {
  switch (type) {
    case RequestType::kGeolocation:
      return vector_icons::kLocationOffChromeRefreshIcon;
    case RequestType::kNotifications:
      return vector_icons::kNotificationsOffChromeRefreshIcon;
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return vector_icons::kVrHeadsetOffChromeRefreshIcon;
    case RequestType::kCameraStream:
      return vector_icons::kVideocamOffChromeRefreshIcon;
    case RequestType::kCapturedSurfaceControl:
      return vector_icons::kTouchpadMouseOffIcon;
    case RequestType::kClipboard:
      return vector_icons::kContentPasteOffIcon;
    case RequestType::kHandTracking:
      return vector_icons::kHandGestureOffIcon;
    case RequestType::kIdleDetection:
      return vector_icons::kDevicesOffIcon;
    case RequestType::kMicStream:
      return vector_icons::kMicOffChromeRefreshIcon;
    case RequestType::kMidiSysex:
      return vector_icons::kMidiOffChromeRefreshIcon;
    case RequestType::kStorageAccess:
      return vector_icons::kStorageAccessOffIcon;
    case RequestType::kIdentityProvider:
      // TODO(crbug.com/40252825): use a dedicated icon
      return gfx::kNoneIcon;
    case RequestType::kKeyboardLock:
      return vector_icons::kKeyboardLockOffIcon;
    case RequestType::kPointerLock:
      return vector_icons::kPointerLockOffIcon;
    case RequestType::kWebAppInstallation:
      // TODO(crbug.com/333795265): provide a dedicated icon.
      return gfx::kNoneIcon;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::kNoneIcon;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

bool IsRequestablePermissionType(ContentSettingsType content_settings_type) {
  return !!ContentSettingsTypeToRequestTypeIfExists(content_settings_type);
}

std::optional<RequestType> ContentSettingsTypeToRequestTypeIfExists(
    ContentSettingsType content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::AR:
      return RequestType::kArSession;
#if !BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return RequestType::kCameraPanTiltZoom;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      return RequestType::kCapturedSurfaceControl;
#endif
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return RequestType::kCameraStream;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return RequestType::kClipboard;
#if !BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::LOCAL_FONTS:
      return RequestType::kLocalFonts;
#endif
    case ContentSettingsType::GEOLOCATION:
      return RequestType::kGeolocation;
    case ContentSettingsType::HAND_TRACKING:
      return RequestType::kHandTracking;
    case ContentSettingsType::IDLE_DETECTION:
      return RequestType::kIdleDetection;
#if !BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::KEYBOARD_LOCK:
      return RequestType::kKeyboardLock;
#endif
    case ContentSettingsType::MEDIASTREAM_MIC:
      return RequestType::kMicStream;
    case ContentSettingsType::MIDI_SYSEX:
      return RequestType::kMidiSysex;
    case ContentSettingsType::NOTIFICATIONS:
      return RequestType::kNotifications;
#if !BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::POINTER_LOCK:
      return RequestType::kPointerLock;
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return RequestType::kProtectedMediaIdentifier;
#endif
#if BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::NFC:
      return RequestType::kNfcDevice;
#endif
    case ContentSettingsType::STORAGE_ACCESS:
      return RequestType::kStorageAccess;
    case ContentSettingsType::VR:
      return RequestType::kVrSession;
#if !BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::WINDOW_MANAGEMENT:
      return RequestType::kWindowManagement;
#endif
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      return RequestType::kTopLevelStorageAccess;
#if !BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      return RequestType::kFileSystemAccess;
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case ContentSettingsType::SMART_CARD_DATA:
      return RequestType::kSmartCard;
#endif
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    case ContentSettingsType::WEB_PRINTING:
      return RequestType::kWebPrinting;
#endif
    case ContentSettingsType::FEDERATED_IDENTITY_API:
      return RequestType::kIdentityProvider;
    default:
      return std::nullopt;
  }
}

RequestType ContentSettingsTypeToRequestType(
    ContentSettingsType content_settings_type) {
  std::optional<RequestType> request_type =
      ContentSettingsTypeToRequestTypeIfExists(content_settings_type);
  CHECK(request_type);
  return *request_type;
}

std::optional<ContentSettingsType> RequestTypeToContentSettingsType(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kArSession:
      return ContentSettingsType::AR;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kCameraPanTiltZoom:
      return ContentSettingsType::CAMERA_PAN_TILT_ZOOM;
#endif
    case RequestType::kCameraStream:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kCapturedSurfaceControl:
      return ContentSettingsType::CAPTURED_SURFACE_CONTROL;
#endif
    case RequestType::kClipboard:
      return ContentSettingsType::CLIPBOARD_READ_WRITE;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kLocalFonts:
      return ContentSettingsType::LOCAL_FONTS;
#endif
    case RequestType::kGeolocation:
      return ContentSettingsType::GEOLOCATION;
    case RequestType::kHandTracking:
      return ContentSettingsType::HAND_TRACKING;
    case RequestType::kIdleDetection:
      return ContentSettingsType::IDLE_DETECTION;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kKeyboardLock:
      return ContentSettingsType::KEYBOARD_LOCK;
#endif
    case RequestType::kMicStream:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case RequestType::kMidiSysex:
      return ContentSettingsType::MIDI_SYSEX;
#if BUILDFLAG(IS_ANDROID)
    case RequestType::kNfcDevice:
      return ContentSettingsType::NFC;
#endif
    case RequestType::kNotifications:
      return ContentSettingsType::NOTIFICATIONS;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kPointerLock:
      return ContentSettingsType::POINTER_LOCK;
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      return ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER;
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case RequestType::kSmartCard:
      return ContentSettingsType::SMART_CARD_DATA;
#endif
    case RequestType::kStorageAccess:
      return ContentSettingsType::STORAGE_ACCESS;
    case RequestType::kVrSession:
      return ContentSettingsType::VR;
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    case RequestType::kWebPrinting:
      return ContentSettingsType::WEB_PRINTING;
#endif
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kWindowManagement:
      return ContentSettingsType::WINDOW_MANAGEMENT;
#endif
    case RequestType::kTopLevelStorageAccess:
      return ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS;
    default:
      // Not associated with a ContentSettingsType.
      return std::nullopt;
  }
}

// Returns whether confirmation chips can be displayed
bool IsConfirmationChipSupported(RequestType for_request_type) {
  static constexpr auto kRequestsWithChip =
      base::MakeFixedFlatSet<RequestType>({
          // clang-format off
          RequestType::kNotifications,
          RequestType::kGeolocation,
          RequestType::kCameraStream,
          RequestType::kMicStream,
          // clang-format on
      });
  return base::Contains(kRequestsWithChip, for_request_type);
}

IconId GetIconId(RequestType type) {
  IconId override_id = PermissionsClient::Get()->GetOverrideIconId(type);
#if BUILDFLAG(IS_ANDROID)
  if (override_id)
    return override_id;
  return GetIconIdAndroid(type);
#else
  if (!override_id.is_empty())
    return override_id;
  return GetIconIdDesktop(type);
#endif
}

#if !BUILDFLAG(IS_ANDROID)
IconId GetBlockedIconId(RequestType type) {
  return GetBlockedIconIdDesktop(type);
}
#endif

const char* PermissionKeyForRequestType(permissions::RequestType request_type) {
  switch (request_type) {
    case permissions::RequestType::kArSession:
      return "ar_session";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kCameraPanTiltZoom:
      return "camera_pan_tilt_zoom";
#endif
    case permissions::RequestType::kCameraStream:
      return "camera_stream";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kCapturedSurfaceControl:
      return "captured_surface_control";
#endif
    case permissions::RequestType::kClipboard:
      return "clipboard";
    case permissions::RequestType::kDiskQuota:
      return "disk_quota";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kFileSystemAccess:
      return "file_system";
#endif
    case permissions::RequestType::kGeolocation:
      return "geolocation";
    case permissions::RequestType::kHandTracking:
      return "hand_tracking";
    case permissions::RequestType::kIdleDetection:
      return "idle_detection";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kKeyboardLock:
      return "keyboard_lock";
    case permissions::RequestType::kLocalFonts:
      return "local_fonts";
#endif
    case permissions::RequestType::kMicStream:
      return "mic_stream";
    case permissions::RequestType::kMidiSysex:
      return "midi_sysex";
    case permissions::RequestType::kMultipleDownloads:
      return "multiple_downloads";
#if BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kNfcDevice:
      return "nfc_device";
#endif
    case permissions::RequestType::kNotifications:
      return "notifications";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kPointerLock:
      return "pointer_lock";
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case permissions::RequestType::kProtectedMediaIdentifier:
      return "protected_media_identifier";
#endif
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kRegisterProtocolHandler:
      return "register_protocol_handler";
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case RequestType::kSmartCard:
      return "smart_card";
#endif
    case permissions::RequestType::kStorageAccess:
      return "storage_access";
    case permissions::RequestType::kTopLevelStorageAccess:
      return "top_level_storage_access";
    case permissions::RequestType::kVrSession:
      return "vr_session";
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    case RequestType::kWebPrinting:
      return "web_printing";
#endif
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kWebAppInstallation:
      return "web_app_installation";
    case permissions::RequestType::kWindowManagement:
      return "window_management";
#endif
    case permissions::RequestType::kIdentityProvider:
      return "identity_provider";
  }

  return nullptr;
}

}  // namespace permissions
