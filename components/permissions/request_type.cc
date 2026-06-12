// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/request_type.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permissions_client.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/resources/android/theme_resources.h"
#else
#include "components/vector_icons/vector_icons.h"
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
    case RequestType::kFileSystemAccess:
      NOTREACHED();
    case RequestType::kGeolocation:
      return IDR_ANDROID_INFOBAR_GEOLOCATION;
    case RequestType::kHandTracking:
      return IDR_ANDROID_INFOBAR_HAND_TRACKING;
    case RequestType::kIdentityProvider:
      return IDR_ANDROID_INFOBAR_IDENTITY_PROVIDER;
    case RequestType::kIdleDetection:
      return IDR_ANDROID_INFOBAR_IDLE_DETECTION;
    case RequestType::kLocalNetwork:
      return IDR_ANDROID_INFOBAR_LOCAL_NETWORK;
    case RequestType::kLoopbackNetwork:
      return IDR_ANDROID_INFOBAR_LOOPBACK_NETWORK;
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
    case RequestType::kSensors:
      return IDR_ANDROID_INFOBAR_SENSORS;
    case RequestType::kProtectedMediaIdentifier:
      return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      return IDR_ANDROID_STORAGE_ACCESS;
    case RequestType::kWindowManagement:
      return IDR_ANDROID_INFOBAR_WINDOW_MANAGEMENT;
  }
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// TODO(crbug.com/335848275): Migrate the icons in 2 steps.
// 1 - Copy contents of refresh icons into current non-refresh icons.
// 2 - In a separate change, remove the refresh icons.
const gfx::VectorIcon& GetIconIdDesktop(RequestType type) {
  switch (type) {
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kCardboardIcon
                 : vector_icons::kVrHeadsetChromeRefreshOldIcon;
    case RequestType::kCameraPanTiltZoom:
    case RequestType::kCameraStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVideocamIcon
                 : vector_icons::kVideocamChromeRefreshOldIcon;
    case RequestType::kCapturedSurfaceControl:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kTouchpadMouseIcon
                 : vector_icons::kTouchpadMouseOldIcon;
    case RequestType::kClipboard:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kContentPasteIcon
                 : vector_icons::kContentPasteOldIcon;
    case RequestType::kDiskQuota:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFolderFlippableIcon
                 : vector_icons::kFolderChromeRefreshOldIcon;
    case RequestType::kGeolocation:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kLocationOnIcon
                 : vector_icons::kLocationOnChromeRefreshOldIcon;
    case RequestType::kHandTracking:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kHandGestureIcon
                 : vector_icons::kHandGestureOldIcon;
    case RequestType::kIdleDetection:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDevicesIcon
                 : vector_icons::kDevicesOldIcon;
    case RequestType::kKeyboardLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kKeyboardLockIcon
                 : vector_icons::kKeyboardLockOldIcon;
    case RequestType::kLocalFonts:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFontDownloadIcon
                 : vector_icons::kFontDownloadChromeRefreshOldIcon;
    case RequestType::kLocalNetwork:
      return ::features::IsRoundedIconsEnabled() ? vector_icons::kRouterIcon
                                                 : vector_icons::kRouterOldIcon;
    case RequestType::kLoopbackNetwork:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDesktopWindowsIcon
                 : vector_icons::kDesktopWindowsOldIcon;
    case RequestType::kMicStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMicIcon
                 : vector_icons::kMicChromeRefreshOldIcon;
    case RequestType::kMidiSysex:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kPianoIcon
                 : vector_icons::kMidiChromeRefreshOldIcon;
    case RequestType::kMultipleDownloads:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDownloadIcon
                 : vector_icons::kFileDownloadChromeRefreshOldIcon;
    case RequestType::kNotifications:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kNotificationsIcon
                 : vector_icons::kNotificationsChromeRefreshOldIcon;
    case RequestType::kPointerLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMouseLockIcon
                 : vector_icons::kPointerLockOldIcon;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      // This icon is provided by ChromePermissionsClient::GetOverrideIconId.
      NOTREACHED();
#endif
    case RequestType::kRegisterProtocolHandler:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kProtocolHandlerIcon
                 : vector_icons::kProtocolHandlerOldIcon;
    case RequestType::kSensors:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kSensorsIcon
                 : vector_icons::kSensorsChromeRefreshOldIcon;
#if BUILDFLAG(IS_CHROMEOS)
    case RequestType::kSmartCard:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kSmartCardReaderIcon
                 : vector_icons::kSmartCardReaderOldIcon;
#endif
    case RequestType::kWebAppInstallation:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kInstallDesktopIcon
                 : vector_icons::kInstallDesktopOldIcon;
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    case RequestType::kWebPrinting:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kPrintIcon
                 : vector_icons::kPrinterOldIcon;
#endif
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVr180Create2dIcon
                 : vector_icons::kStorageAccessOldIcon;
    case RequestType::kWindowManagement:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kSelectWindowIcon
                 : vector_icons::kSelectWindowChromeRefreshOldIcon;
    case RequestType::kFileSystemAccess:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFolderFilledIcon
                 : vector_icons::kFolderOldIcon;
    case RequestType::kIdentityProvider:
      // TODO(crbug.com/40252825): provide a dedicated icon.
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kFolderFilledIcon
                 : vector_icons::kFolderOldIcon;
  }
  NOTREACHED();
}

const gfx::VectorIcon& GetBlockedIconIdDesktop(RequestType type) {
  switch (type) {
    case RequestType::kGeolocation:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kLocationOffIcon
                 : vector_icons::kLocationOffChromeRefreshOldIcon;
    case RequestType::kNotifications:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kNotificationsOffIcon
                 : vector_icons::kNotificationsOffChromeRefreshOldIcon;
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kCardboardOffIcon
                 : vector_icons::kVrHeadsetOffChromeRefreshOldIcon;
    case RequestType::kCameraStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVideocamOffIcon
                 : vector_icons::kVideocamOffChromeRefreshOldIcon;
    case RequestType::kCapturedSurfaceControl:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kTouchpadMouseOffIcon
                 : vector_icons::kTouchpadMouseOffOldIcon;
    case RequestType::kClipboard:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kContentPasteOffIcon
                 : vector_icons::kContentPasteOffOldIcon;
    case RequestType::kHandTracking:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kHandGestureOffIcon
                 : vector_icons::kHandGestureOffOldIcon;
    case RequestType::kIdleDetection:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDevicesOffIcon
                 : vector_icons::kDevicesOffOldIcon;
    case RequestType::kLocalNetwork:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kRouterOffIcon
                 : vector_icons::kRouterOffOldIcon;
    case RequestType::kLoopbackNetwork:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kDesktopAccessDisabledIcon
                 : vector_icons::kDesktopAccessDisabledOldIcon;
    case RequestType::kMicStream:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMicOffIcon
                 : vector_icons::kMicOffChromeRefreshOldIcon;
    case RequestType::kMidiSysex:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kPianoOffIcon
                 : vector_icons::kMidiOffChromeRefreshOldIcon;
    case RequestType::kSensors:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kSensorsOffIcon
                 : vector_icons::kSensorsOffChromeRefreshOldIcon;
    case RequestType::kStorageAccess:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kVr180Create2dOffIcon
                 : vector_icons::kStorageAccessOffOldIcon;
    case RequestType::kIdentityProvider:
      // TODO(crbug.com/40252825): use a dedicated icon
      return gfx::VectorIcon::EmptyIcon();
    case RequestType::kKeyboardLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kKeyboardLockOffIcon
                 : vector_icons::kKeyboardLockOffOldIcon;
    case RequestType::kPointerLock:
      return ::features::IsRoundedIconsEnabled()
                 ? vector_icons::kMouseLockOffIcon
                 : vector_icons::kPointerLockOffOldIcon;
    case RequestType::kWebAppInstallation:
      return vector_icons::kInstallDesktopOffCustomIcon;
    default:
      NOTREACHED();
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

bool IsRequestablePermissionType(ContentSettingsType content_settings_type) {
  return !!ContentSettingsTypeToRequestTypeIfExists(content_settings_type);
}

std::optional<RequestType> ContentSettingsTypeToRequestTypeIfExists(
    ContentSettingsType content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::AR:
      return RequestType::kArSession;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return RequestType::kCameraPanTiltZoom;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      return RequestType::kCapturedSurfaceControl;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return RequestType::kCameraStream;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return RequestType::kClipboard;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::LOCAL_FONTS:
      return RequestType::kLocalFonts;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::GEOLOCATION:
    case ContentSettingsType::GEOLOCATION_WITH_OPTIONS:
      return RequestType::kGeolocation;
    case ContentSettingsType::HAND_TRACKING:
      return RequestType::kHandTracking;
    case ContentSettingsType::IDLE_DETECTION:
      return RequestType::kIdleDetection;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::KEYBOARD_LOCK:
      return RequestType::kKeyboardLock;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::MEDIASTREAM_MIC:
      return RequestType::kMicStream;
    case ContentSettingsType::MIDI_SYSEX:
      return RequestType::kMidiSysex;
    case ContentSettingsType::NOTIFICATIONS:
      return RequestType::kNotifications;
    case ContentSettingsType::SENSORS:
      return RequestType::kSensors;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::POINTER_LOCK:
      return RequestType::kPointerLock;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return RequestType::kProtectedMediaIdentifier;
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    case ContentSettingsType::NFC:
      return RequestType::kNfcDevice;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    case ContentSettingsType::STORAGE_ACCESS:
      return RequestType::kStorageAccess;
    case ContentSettingsType::VR:
      return RequestType::kVrSession;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      return RequestType::kWindowManagement;
    case ContentSettingsType::LOCAL_NETWORK:
      return RequestType::kLocalNetwork;
    case ContentSettingsType::LOOPBACK_NETWORK:
      return RequestType::kLoopbackNetwork;
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      return RequestType::kTopLevelStorageAccess;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      return RequestType::kFileSystemAccess;
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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case ContentSettingsType::WEB_APP_INSTALLATION:
      return RequestType::kWebAppInstallation;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kCameraPanTiltZoom:
      return ContentSettingsType::CAMERA_PAN_TILT_ZOOM;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kCameraStream:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kCapturedSurfaceControl:
      return ContentSettingsType::CAPTURED_SURFACE_CONTROL;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kClipboard:
      return ContentSettingsType::CLIPBOARD_READ_WRITE;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kLocalFonts:
      return ContentSettingsType::LOCAL_FONTS;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kLocalNetwork:
      return ContentSettingsType::LOCAL_NETWORK;
    case RequestType::kLoopbackNetwork:
      return ContentSettingsType::LOOPBACK_NETWORK;
    case RequestType::kGeolocation:
      return content_settings::GeolocationContentSettingsType();
    case RequestType::kHandTracking:
      return ContentSettingsType::HAND_TRACKING;
    case RequestType::kIdleDetection:
      return ContentSettingsType::IDLE_DETECTION;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kKeyboardLock:
      return ContentSettingsType::KEYBOARD_LOCK;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kMicStream:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case RequestType::kMidiSysex:
      return ContentSettingsType::MIDI_SYSEX;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    case RequestType::kNfcDevice:
      return ContentSettingsType::NFC;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    case RequestType::kNotifications:
      return ContentSettingsType::NOTIFICATIONS;
    case RequestType::kSensors:
      return ContentSettingsType::SENSORS;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kPointerLock:
      return ContentSettingsType::POINTER_LOCK;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
    case RequestType::kWindowManagement:
      return ContentSettingsType::WINDOW_MANAGEMENT;
    case RequestType::kTopLevelStorageAccess:
      return ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kWebAppInstallation:
      return ContentSettingsType::WEB_APP_INSTALLATION;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      // Not associated with a ContentSettingsType.
    case RequestType::kDiskQuota:
    case RequestType::kFileSystemAccess:
    case RequestType::kIdentityProvider:
    case RequestType::kMultipleDownloads:
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case RequestType::kRegisterProtocolHandler:
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
          RequestType::kSensors,
          // clang-format on
      });
  return kRequestsWithChip.contains(for_request_type);
}

#if !BUILDFLAG(IS_IOS)
IconId GetIconId(RequestType type) {
  IconId override_id = PermissionsClient::Get()->GetOverrideIconId(type);
#if BUILDFLAG(IS_ANDROID)
  if (override_id) {
    return override_id;
  }
  return GetIconIdAndroid(type);
#else
  if (!override_id.is_empty()) {
    return override_id;
  }
  return GetIconIdDesktop(type);
#endif
}
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
IconId GetBlockedIconId(RequestType type) {
  return GetBlockedIconIdDesktop(type);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

const char* PermissionKeyForRequestType(permissions::RequestType request_type) {
  switch (request_type) {
    case permissions::RequestType::kArSession:
      return "ar_session";
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kCameraPanTiltZoom:
      return "camera_pan_tilt_zoom";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kCameraStream:
      return "camera_stream";
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kCapturedSurfaceControl:
      return "captured_surface_control";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kClipboard:
      return "clipboard";
    case permissions::RequestType::kDiskQuota:
      return "disk_quota";
    case permissions::RequestType::kFileSystemAccess:
      return "file_system";
    case permissions::RequestType::kGeolocation:
      return "geolocation";
    case permissions::RequestType::kHandTracking:
      return "hand_tracking";
    case permissions::RequestType::kIdleDetection:
      return "idle_detection";
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kKeyboardLock:
      return "keyboard_lock";
    case permissions::RequestType::kLocalFonts:
      return "local_fonts";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kLocalNetwork:
      return "local_network";
    case permissions::RequestType::kLoopbackNetwork:
      return "loopback_network";
    case permissions::RequestType::kMicStream:
      return "mic_stream";
    case permissions::RequestType::kMidiSysex:
      return "midi_sysex";
    case permissions::RequestType::kMultipleDownloads:
      return "multiple_downloads";
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    case permissions::RequestType::kNfcDevice:
      return "nfc_device";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    case permissions::RequestType::kNotifications:
      return "notifications";
    case permissions::RequestType::kSensors:
      return "sensors";
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kPointerLock:
      return "pointer_lock";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case permissions::RequestType::kProtectedMediaIdentifier:
      return "protected_media_identifier";
#endif
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kRegisterProtocolHandler:
      return "register_protocol_handler";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kWebAppInstallation:
      return "web_app_installation";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    case permissions::RequestType::kWindowManagement:
      return "window_management";
    case permissions::RequestType::kIdentityProvider:
      return "identity_provider";
  }

  return nullptr;
}

}  // namespace permissions
