// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/request_type.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permissions_client.h"
#include "ui/base/ui_base_features.h"

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
    case RequestType::kAccessibilityEvents:
      return IDR_ANDROID_INFOBAR_ACCESSIBILITY_EVENTS;
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
    case RequestType::kIdleDetection:
      return IDR_ANDROID_INFOBAR_IDLE_DETECTION;
    case RequestType::kMicStream:
      return IDR_ANDROID_INFOBAR_MEDIA_STREAM_MIC;
    case RequestType::kMidi:
      // kMidi and kMidiSysex share the same Android icon ID.
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
      return IDR_ANDROID_INFOBAR_PERMISSION_COOKIE;
  }
  NOTREACHED();
  return 0;
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
const gfx::VectorIcon& GetIconIdDesktop(RequestType type) {
  const bool cr23 = ::features::IsChromeRefresh2023();
  switch (type) {
    case RequestType::kAccessibilityEvents:
      return kAccessibilityIcon;
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return cr23 ? vector_icons::kVrHeadsetChromeRefreshIcon
                  : vector_icons::kVrHeadsetIcon;
    case RequestType::kCameraPanTiltZoom:
    case RequestType::kCameraStream:
      return cr23 ? vector_icons::kVideocamChromeRefreshIcon
                  : vector_icons::kVideocamIcon;
    case RequestType::kClipboard:
      return cr23 ? vector_icons::kContentPasteChromeRefreshIcon
                  : vector_icons::kContentPasteIcon;
    case RequestType::kDiskQuota:
      return cr23 ? vector_icons::kFolderChromeRefreshIcon
                  : vector_icons::kFolderIcon;
    case RequestType::kGeolocation:
      return cr23 ? vector_icons::kLocationOnChromeRefreshIcon
                  : vector_icons::kLocationOnIcon;
    case RequestType::kIdleDetection:
      return cr23 ? vector_icons::kDevicesChromeRefreshIcon
                  : vector_icons::kDevicesIcon;
    case RequestType::kLocalFonts:
      return cr23 ? vector_icons::kFontDownloadChromeRefreshIcon
                  : vector_icons::kFontDownloadIcon;
    case RequestType::kMicStream:
      return cr23 ? vector_icons::kMicChromeRefreshIcon
                  : vector_icons::kMicIcon;
    case RequestType::kMidi:
      // kMidi and kMidiSysex share the same desktop icon ID.
    case RequestType::kMidiSysex:
      return cr23 ? vector_icons::kMidiChromeRefreshIcon
                  : vector_icons::kMidiIcon;
    case RequestType::kMultipleDownloads:
      return cr23 ? vector_icons::kFileDownloadChromeRefreshIcon
                  : vector_icons::kFileDownloadIcon;
    case RequestType::kNotifications:
      return cr23 ? vector_icons::kNotificationsChromeRefreshIcon
                  : vector_icons::kNotificationsIcon;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      // This icon is provided by ChromePermissionsClient::GetOverrideIconId.
      NOTREACHED();
      return gfx::kNoneIcon;
#endif
    case RequestType::kRegisterProtocolHandler:
      return vector_icons::kProtocolHandlerIcon;
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      return vector_icons::kStorageAccessIcon;
    case RequestType::kWindowManagement:
      return cr23 ? vector_icons::kSelectWindowChromeRefreshIcon
                  : vector_icons::kSelectWindowIcon;
  }
  NOTREACHED();
  return gfx::kNoneIcon;
}

const gfx::VectorIcon& GetBlockedIconIdDesktop(RequestType type) {
  const bool cr23 = ::features::IsChromeRefresh2023();
  switch (type) {
    case RequestType::kGeolocation:
      return cr23 ? vector_icons::kLocationOffChromeRefreshIcon
                  : vector_icons::kLocationOffIcon;
    case RequestType::kNotifications:
      return cr23 ? vector_icons::kNotificationsOffChromeRefreshIcon
                  : vector_icons::kNotificationsOffIcon;
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return cr23 ? vector_icons::kVrHeadsetOffChromeRefreshIcon
                  : vector_icons::kVrHeadsetOffIcon;
    case RequestType::kCameraStream:
      return cr23 ? vector_icons::kVideocamOffChromeRefreshIcon
                  : vector_icons::kVideocamOffIcon;
    case RequestType::kClipboard:
      return cr23 ? vector_icons::kContentPasteOffChromeRefreshIcon
                  : vector_icons::kContentPasteOffIcon;
    case RequestType::kIdleDetection:
      return cr23 ? vector_icons::kDevicesOffChromeRefreshIcon
                  : vector_icons::kDevicesOffIcon;
    case RequestType::kMicStream:
      return cr23 ? vector_icons::kMicOffChromeRefreshIcon
                  : vector_icons::kMicOffIcon;
    case RequestType::kMidi:
      // kMidi and kMidiSysex share the same desktop block icon ID.
    case RequestType::kMidiSysex:
      return cr23 ? vector_icons::kMidiOffChromeRefreshIcon
                  : vector_icons::kMidiOffIcon;
    case RequestType::kStorageAccess:
      return vector_icons::kStorageAccessOffIcon;
    default:
      NOTREACHED();
  }
  NOTREACHED();
  return gfx::kNoneIcon;
}
#endif  // !BUILDFLAG(IS_ANDROID)

absl::optional<RequestType> ContentSettingsTypeToRequestTypeIfExists(
    ContentSettingsType content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      return RequestType::kAccessibilityEvents;
    case ContentSettingsType::AR:
      return RequestType::kArSession;
#if !BUILDFLAG(IS_ANDROID)
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return RequestType::kCameraPanTiltZoom;
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
    case ContentSettingsType::IDLE_DETECTION:
      return RequestType::kIdleDetection;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return RequestType::kMicStream;
    case ContentSettingsType::MIDI:
      if (base::FeatureList::IsEnabled(features::kBlockMidiByDefault)) {
        return RequestType::kMidi;
      } else {
        return absl::nullopt;
      }
    case ContentSettingsType::MIDI_SYSEX:
      return RequestType::kMidiSysex;
    case ContentSettingsType::NOTIFICATIONS:
      return RequestType::kNotifications;
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
    default:
      return absl::nullopt;
  }
}

}  // namespace

bool IsRequestablePermissionType(ContentSettingsType content_settings_type) {
  return !!ContentSettingsTypeToRequestTypeIfExists(content_settings_type);
}

RequestType ContentSettingsTypeToRequestType(
    ContentSettingsType content_settings_type) {
  absl::optional<RequestType> request_type =
      ContentSettingsTypeToRequestTypeIfExists(content_settings_type);
  CHECK(request_type);
  return *request_type;
}

absl::optional<ContentSettingsType> RequestTypeToContentSettingsType(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kAccessibilityEvents:
      return ContentSettingsType::ACCESSIBILITY_EVENTS;
    case RequestType::kArSession:
      return ContentSettingsType::AR;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kCameraPanTiltZoom:
      return ContentSettingsType::CAMERA_PAN_TILT_ZOOM;
#endif
    case RequestType::kCameraStream:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
    case RequestType::kClipboard:
      return ContentSettingsType::CLIPBOARD_READ_WRITE;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kLocalFonts:
      return ContentSettingsType::LOCAL_FONTS;
#endif
    case RequestType::kGeolocation:
      return ContentSettingsType::GEOLOCATION;
    case RequestType::kIdleDetection:
      return ContentSettingsType::IDLE_DETECTION;
    case RequestType::kMicStream:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case RequestType::kMidi:
      if (base::FeatureList::IsEnabled(features::kBlockMidiByDefault)) {
        return ContentSettingsType::MIDI;
      } else {
        return absl::nullopt;
      }
    case RequestType::kMidiSysex:
      return ContentSettingsType::MIDI_SYSEX;
#if BUILDFLAG(IS_ANDROID)
    case RequestType::kNfcDevice:
      return ContentSettingsType::NFC;
#endif
    case RequestType::kNotifications:
      return ContentSettingsType::NOTIFICATIONS;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      return ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER;
#endif
    case RequestType::kStorageAccess:
      return ContentSettingsType::STORAGE_ACCESS;
    case RequestType::kVrSession:
      return ContentSettingsType::VR;
#if !BUILDFLAG(IS_ANDROID)
    case RequestType::kWindowManagement:
      return ContentSettingsType::WINDOW_MANAGEMENT;
#endif
    case RequestType::kTopLevelStorageAccess:
      return ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS;
    default:
      // Not associated with a ContentSettingsType.
      return absl::nullopt;
  }
}

// Returns whether confirmation chips can be displayed
bool IsConfirmationChipSupported(RequestType for_request_type) {
  return base::ranges::any_of(
      std::vector<RequestType>{
          RequestType::kNotifications, RequestType::kGeolocation,
          RequestType::kCameraStream, RequestType::kMicStream},
      [for_request_type](permissions::RequestType request_type) {
        return request_type == for_request_type;
      });
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
    case permissions::RequestType::kAccessibilityEvents:
      return "accessibility_events";
    case permissions::RequestType::kArSession:
      return "ar_session";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kCameraPanTiltZoom:
      return "camera_pan_tilt_zoom";
#endif
    case permissions::RequestType::kCameraStream:
      return "camera_stream";
    case permissions::RequestType::kClipboard:
      return "clipboard";
    case permissions::RequestType::kDiskQuota:
      return "disk_quota";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kLocalFonts:
      return "local_fonts";
#endif
    case permissions::RequestType::kGeolocation:
      return "geolocation";
    case permissions::RequestType::kIdleDetection:
      return "idle_detection";
    case permissions::RequestType::kMicStream:
      return "mic_stream";
    case permissions::RequestType::kMidi:
      if (base::FeatureList::IsEnabled(features::kBlockMidiByDefault)) {
        return "midi";
      } else {
        return nullptr;
      }
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
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case permissions::RequestType::kProtectedMediaIdentifier:
      return "protected_media_identifier";
#endif
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kRegisterProtocolHandler:
      return "register_protocol_handler";
#endif
    case permissions::RequestType::kStorageAccess:
      return "storage_access";
    case permissions::RequestType::kTopLevelStorageAccess:
      return "top_level_storage_access";
    case permissions::RequestType::kVrSession:
      return "vr_session";
#if !BUILDFLAG(IS_ANDROID)
    case permissions::RequestType::kWindowManagement:
      if (base::FeatureList::IsEnabled(
              features::kWindowManagementPermissionAlias)) {
        return "window_management";
      } else {
        return "window_placement";
      }
#endif
  }

  return nullptr;
}

}  // namespace permissions
