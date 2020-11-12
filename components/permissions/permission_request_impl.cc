// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "components/resources/android/theme_resources.h"
#include "media/base/android/media_drm_bridge.h"
#else
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"
#endif

namespace permissions {

PermissionRequestImpl::PermissionRequestImpl(
    const GURL& request_origin,
    ContentSettingsType content_settings_type,
    bool has_gesture,
    PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure delete_callback)
    : request_origin_(request_origin),
      content_settings_type_(content_settings_type),
      has_gesture_(has_gesture),
      permission_decided_callback_(std::move(permission_decided_callback)),
      delete_callback_(std::move(delete_callback)) {}

PermissionRequestImpl::~PermissionRequestImpl() {
  DCHECK(delete_callback_.is_null());
}

PermissionRequest::IconId PermissionRequestImpl::GetIconId() const {
  PermissionRequest::IconId icon_id =
      PermissionsClient::Get()->GetOverrideIconId(content_settings_type_);
#if defined(OS_ANDROID)
  if (icon_id)
    return icon_id;
  switch (content_settings_type_) {
    case ContentSettingsType::GEOLOCATION:
      return IDR_ANDROID_INFOBAR_GEOLOCATION;
    case ContentSettingsType::NOTIFICATIONS:
      return IDR_ANDROID_INFOBAR_NOTIFICATIONS;
    case ContentSettingsType::MIDI_SYSEX:
      return IDR_ANDROID_INFOBAR_MIDI;
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return IDR_ANDROID_INFOBAR_MEDIA_STREAM_MIC;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA;
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      return IDR_ANDROID_INFOBAR_ACCESSIBILITY_EVENTS;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return IDR_ANDROID_INFOBAR_CLIPBOARD;
    case ContentSettingsType::NFC:
      return IDR_ANDROID_INFOBAR_NFC;
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
      return IDR_ANDROID_INFOBAR_VR_HEADSET;
    case ContentSettingsType::STORAGE_ACCESS:
      return IDR_ANDROID_INFOBAR_PERMISSION_COOKIE;
    case ContentSettingsType::IDLE_DETECTION:
      return IDR_ANDROID_INFOBAR_IDLE_DETECTION;
    default:
      NOTREACHED();
      return IDR_ANDROID_INFOBAR_WARNING;
  }
#else
  if (!icon_id.is_empty())
    return icon_id;
  switch (content_settings_type_) {
    case ContentSettingsType::GEOLOCATION:
      return vector_icons::kLocationOnIcon;
    case ContentSettingsType::NOTIFICATIONS:
      return vector_icons::kNotificationsIcon;
    case ContentSettingsType::MIDI_SYSEX:
      return vector_icons::kMidiIcon;
    case ContentSettingsType::PLUGINS:
      return vector_icons::kExtensionIcon;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return vector_icons::kMicIcon;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return vector_icons::kVideocamIcon;
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      return vector_icons::kAccessibilityIcon;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return vector_icons::kContentPasteIcon;
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
      return vector_icons::kVrHeadsetIcon;
    case ContentSettingsType::STORAGE_ACCESS:
      return vector_icons::kCookieIcon;
    case ContentSettingsType::WINDOW_PLACEMENT:
      return vector_icons::kWindowPlacementIcon;
    case ContentSettingsType::FONT_ACCESS:
      return vector_icons::kFontDownloadIcon;
    case ContentSettingsType::IDLE_DETECTION:
      return vector_icons::kPersonIcon;
    default:
      NOTREACHED();
      return vector_icons::kExtensionIcon;
  }
#endif
}

#if defined(OS_ANDROID)
base::string16 PermissionRequestImpl::GetMessageText() const {
  int message_id;
  switch (content_settings_type_) {
    case ContentSettingsType::GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_TEXT;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      message_id = IDS_NOTIFICATIONS_INFOBAR_TEXT;
      break;
    case ContentSettingsType::MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_INFOBAR_TEXT;
      break;
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      message_id =
          media::MediaDrmBridge::IsPerOriginProvisioningSupported()
              ? IDS_PROTECTED_MEDIA_IDENTIFIER_PER_ORIGIN_PROVISIONING_INFOBAR_TEXT
              : IDS_PROTECTED_MEDIA_IDENTIFIER_PER_DEVICE_PROVISIONING_INFOBAR_TEXT;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_INFOBAR_TEXT;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_INFOBAR_TEXT;
      break;
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      message_id = IDS_ACCESSIBILITY_EVENTS_INFOBAR_TEXT;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      message_id = IDS_CLIPBOARD_INFOBAR_TEXT;
      break;
    case ContentSettingsType::NFC:
      message_id = IDS_NFC_INFOBAR_TEXT;
      break;
    case ContentSettingsType::VR:
      message_id = IDS_VR_INFOBAR_TEXT;
      break;
    case ContentSettingsType::AR:
      message_id = IDS_AR_INFOBAR_TEXT;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      message_id = IDS_IDLE_DETECTION_INFOBAR_TEXT;
      break;
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringFUTF16(
      message_id,
      url_formatter::FormatUrlForSecurityDisplay(
          GetOrigin(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

base::string16 PermissionRequestImpl::GetQuietTitleText() const {
  if (content_settings_type_ == ContentSettingsType::NOTIFICATIONS) {
    return l10n_util::GetStringFUTF16(
        IDS_NOTIFICATION_QUIET_PERMISSION_PROMPT_TITLE,
        url_formatter::FormatUrlForSecurityDisplay(
            GetOrigin(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  }

  NOTREACHED();
  return base::string16();
}

base::string16 PermissionRequestImpl::GetQuietMessageText() const {
  if (content_settings_type_ == ContentSettingsType::NOTIFICATIONS) {
    return l10n_util::GetStringUTF16(
        IDS_NOTIFICATION_QUIET_PERMISSION_PROMPT_MESSAGE);
  }

  NOTREACHED();
  return GetMessageText();
}
#endif

base::string16 PermissionRequestImpl::GetMessageTextFragment() const {
  int message_id;
  switch (content_settings_type_) {
    case ContentSettingsType::GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
    case ContentSettingsType::MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT;
      break;
#endif
    case ContentSettingsType::PLUGINS:
      message_id = IDS_FLASH_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      message_id = IDS_MEDIA_CAPTURE_CAMERA_PAN_TILT_ZOOM_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      message_id = IDS_ACCESSIBILITY_EVENTS_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      message_id = IDS_CLIPBOARD_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::NFC:
      message_id = IDS_NFC_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::VR:
      message_id = IDS_VR_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::AR:
      message_id = IDS_AR_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      message_id = IDS_STORAGE_ACCESS_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::WINDOW_PLACEMENT:
      message_id = IDS_WINDOW_PLACEMENT_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::FONT_ACCESS:
      message_id = IDS_FONT_ACCESS_PERMISSION_FRAGMENT;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      message_id = IDS_IDLE_DETECTION_PERMISSION_FRAGMENT;
      break;
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringUTF16(message_id);
}

#if !defined(OS_ANDROID)
base::Optional<base::string16> PermissionRequestImpl::GetChipText() const {
  int message_id;
  switch (content_settings_type_) {
    case ContentSettingsType::GEOLOCATION:
      message_id = IDS_GEOLOCATION_PERMISSION_CHIP;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      message_id = IDS_NOTIFICATION_PERMISSIONS_CHIP;
      break;
    case ContentSettingsType::MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_PERMISSION_CHIP;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_CHIP;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_CHIP;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      message_id = IDS_CLIPBOARD_PERMISSION_CHIP;
      break;
    case ContentSettingsType::VR:
      message_id = IDS_VR_PERMISSION_CHIP;
      break;
    case ContentSettingsType::AR:
      message_id = IDS_AR_PERMISSION_CHIP;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      message_id = IDS_IDLE_DETECTION_PERMISSION_CHIP;
      break;
    default:
      // TODO(bsep): We don't actually want to support having no string in the
      // long term, but writing them takes time. In the meantime, we fall back
      // to the existing UI when the string is missing.
      return base::nullopt;
  }
  return l10n_util::GetStringUTF16(message_id);
}
#endif

GURL PermissionRequestImpl::GetOrigin() const {
  return request_origin_;
}

void PermissionRequestImpl::PermissionGranted() {
  std::move(permission_decided_callback_).Run(CONTENT_SETTING_ALLOW);
}

void PermissionRequestImpl::PermissionDenied() {
  std::move(permission_decided_callback_).Run(CONTENT_SETTING_BLOCK);
}

void PermissionRequestImpl::Cancelled() {
  std::move(permission_decided_callback_).Run(CONTENT_SETTING_DEFAULT);
}

void PermissionRequestImpl::RequestFinished() {
  std::move(delete_callback_).Run();
}

PermissionRequestType PermissionRequestImpl::GetPermissionRequestType() const {
  return PermissionUtil::GetRequestType(content_settings_type_);
}

PermissionRequestGestureType PermissionRequestImpl::GetGestureType() const {
  return PermissionUtil::GetGestureType(has_gesture_);
}

ContentSettingsType PermissionRequestImpl::GetContentSettingsType() const {
  return content_settings_type_;
}

}  // namespace permissions
