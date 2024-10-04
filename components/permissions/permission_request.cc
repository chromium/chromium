// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"

namespace permissions {

PermissionRequest::PermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    bool has_gesture,
    PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure delete_callback)
    : data_(
          PermissionRequestData(request_type, has_gesture, requesting_origin)),
      permission_decided_callback_(std::move(permission_decided_callback)),
      delete_callback_(std::move(delete_callback)) {}

PermissionRequest::PermissionRequest(
    PermissionRequestData request_data,
    PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure delete_callback,
    bool uses_automatic_embargo)
    : data_(std::move(request_data)),
      permission_decided_callback_(std::move(permission_decided_callback)),
      delete_callback_(std::move(delete_callback)),
      uses_automatic_embargo_(uses_automatic_embargo) {}

PermissionRequest::~PermissionRequest() {
  DCHECK(delete_callback_.is_null());
}

RequestType PermissionRequest::request_type() const {
  CHECK(data_.request_type);
  return data_.request_type.value();
}

bool PermissionRequest::IsDuplicateOf(PermissionRequest* other_request) const {
  return request_type() == other_request->request_type() &&
         requesting_origin() == other_request->requesting_origin();
}

base::WeakPtr<PermissionRequest> PermissionRequest::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

#if BUILDFLAG(IS_ANDROID)
PermissionRequest::AnnotatedMessageText::AnnotatedMessageText(
    std::u16string text,
    std::vector<std::pair<size_t, size_t>> bolded_ranges)
    : text(text), bolded_ranges(bolded_ranges) {}

PermissionRequest::AnnotatedMessageText::~AnnotatedMessageText() = default;

PermissionRequest::AnnotatedMessageText
PermissionRequest::GetDialogAnnotatedMessageText(
    const GURL& embedding_origin) const {
  int message_id = 0;
  std::u16string requesting_origin_string_formatted =
      url_formatter::FormatUrlForSecurityDisplay(
          requesting_origin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  std::u16string embedding_origin_string_formatted =
      url_formatter::FormatUrlForSecurityDisplay(
          embedding_origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  switch (request_type()) {
    case RequestType::kArSession:
      message_id = IDS_AR_INFOBAR_TEXT;
      break;
    case RequestType::kCameraStream:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_INFOBAR_TEXT;
      break;
    case RequestType::kClipboard:
      message_id = IDS_CLIPBOARD_INFOBAR_TEXT;
      break;
    case RequestType::kDiskQuota:
      // Handled by an override in `QuotaPermissionRequest`.
      NOTREACHED_IN_MIGRATION();
      break;
    case RequestType::kHandTracking:
      message_id = IDS_HAND_TRACKING_INFOBAR_TEXT;
      break;
    case RequestType::kGeolocation:
      message_id = IDS_GEOLOCATION_INFOBAR_TEXT;
      break;
    case RequestType::kIdleDetection:
      message_id = IDS_IDLE_DETECTION_INFOBAR_TEXT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_INFOBAR_TEXT;
      break;
    case RequestType::kMidiSysex:
      message_id = IDS_MIDI_SYSEX_INFOBAR_TEXT;
      break;
    case RequestType::kMultipleDownloads:
      message_id = IDS_MULTI_DOWNLOAD_WARNING;
      break;
    case RequestType::kNfcDevice:
      message_id = IDS_NFC_INFOBAR_TEXT;
      break;
    case RequestType::kNotifications:
      message_id = IDS_NOTIFICATIONS_INFOBAR_TEXT;
      break;
    case RequestType::kProtectedMediaIdentifier:
      message_id =
          IDS_PROTECTED_MEDIA_IDENTIFIER_PER_ORIGIN_PROVISIONING_INFOBAR_TEXT;
      break;
    case RequestType::kStorageAccess:
      // The SA prompt does not currently bold any part of its message.
      return AnnotatedMessageText(
          l10n_util::GetStringFUTF16(
              IDS_CONCAT_TWO_STRINGS_WITH_PERIODS,
              l10n_util::GetStringFUTF16(
                  IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_PROMPT_TITLE,
                  requesting_origin_string_formatted),
              l10n_util::GetStringFUTF16(
                  IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_EXPLANATION,
                  requesting_origin_string_formatted,
                  embedding_origin_string_formatted)),
          /*bolded_ranges=*/{});
    case RequestType::kTopLevelStorageAccess:
      NOTREACHED_IN_MIGRATION();
      break;
    case RequestType::kVrSession:
      message_id = IDS_VR_INFOBAR_TEXT;
      break;
    case RequestType::kIdentityProvider:
      message_id = IDS_IDENTITY_PROVIDER_INFOBAR_TEXT;
      break;
  }
  DCHECK_NE(0, message_id);

  // Only format origins bold iff it's one time allowable (which uses a new
  // prompt design on Clank)
  return GetDialogAnnotatedMessageText(
      requesting_origin_string_formatted, message_id, /*format_origin_bold=*/
      permissions::PermissionUtil::DoesSupportTemporaryGrants(
          GetContentSettingsType()));
}

// static
PermissionRequest::AnnotatedMessageText
PermissionRequest::GetDialogAnnotatedMessageText(
    std::u16string requesting_origin_formatted_for_display,
    int message_id,
    bool format_origin_bold) {
  std::vector<size_t> offsets;
  std::u16string text = l10n_util::GetStringFUTF16(
      message_id, {requesting_origin_formatted_for_display}, &offsets);

  std::vector<std::pair<size_t, size_t>> bolded_ranges;
  if (format_origin_bold) {
    for (auto offset : offsets) {
      bolded_ranges.emplace_back(
          offset, offset + requesting_origin_formatted_for_display.length());
    }
  }

  return AnnotatedMessageText(text, bolded_ranges);
}
#endif

bool PermissionRequest::IsEmbeddedPermissionElementInitiated() const {
  return data_.embedded_permission_element_initiated;
}

std::optional<gfx::Rect> PermissionRequest::GetAnchorElementPosition() const {
  return data_.anchor_element_position;
}

#if !BUILDFLAG(IS_ANDROID)

bool PermissionRequest::IsConfirmationChipSupported() {
  return permissions::IsConfirmationChipSupported(request_type());
}

IconId PermissionRequest::GetIconForChip() {
  return permissions::GetIconId(request_type());
}

IconId PermissionRequest::GetBlockedIconForChip() {
  return permissions::GetBlockedIconId(request_type());
}

std::optional<std::u16string> PermissionRequest::GetRequestChipText(
    ChipTextType type) const {
  static base::NoDestructor<std::map<RequestType, std::vector<int>>> kMessageIds(
      {{RequestType::kArSession,
        {IDS_AR_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kCameraStream,
        {IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_CAMERA_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_CAMERA_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_CAMERA_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kCapturedSurfaceControl,
        {IDS_CAPTURED_SURFACE_CONTROL_PERMISSION_CHIP,
         IDS_CAPTURED_SURFACE_CONTROL_PERMISSION_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_CAPTURED_SURFACE_CONTROL_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_CAPTURED_SURFACE_CONTROL_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kClipboard,
        {IDS_CLIPBOARD_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kGeolocation,
        {IDS_GEOLOCATION_PERMISSION_CHIP,
         IDS_GEOLOCATION_PERMISSION_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_GEOLOCATION_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_GEOLOCATION_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kHandTracking,
        {IDS_HAND_TRACKING_PERMISSION_CHIP,
         IDS_HAND_TRACKING_PERMISSION_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_HAND_TRACKING_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_HAND_TRACKING_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_HAND_TRACKING_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kIdleDetection,
        {IDS_IDLE_DETECTION_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kKeyboardLock,
        {IDS_KEYBOARD_LOCK_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_KEYBOARD_LOCK_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_KEYBOARD_LOCK_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kMicStream,
        {IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_MICROPHONE_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_MICROPHONE_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_MICROPHONE_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kMidiSysex,
        {IDS_MIDI_SYSEX_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kNotifications,
        {IDS_NOTIFICATION_PERMISSIONS_CHIP,
         IDS_NOTIFICATION_PERMISSIONS_BLOCKED_CHIP,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION, -1,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_NOTIFICATION_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         -1,
         IDS_PERMISSIONS_NOTIFICATION_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kPointerLock,
        {IDS_POINTER_LOCK_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_POINTER_LOCK_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_POINTER_LOCK_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kStorageAccess,
        {IDS_SAA_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION, -1,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_SAA_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT, -1,
         IDS_PERMISSIONS_SAA_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}},
       {RequestType::kVrSession,
        {IDS_VR_PERMISSION_CHIP, -1, -1, -1, -1, -1, -1, -1}},
       {RequestType::kWebAppInstallation,
        {IDS_WEB_APP_INSTALLATION_PERMISSION_CHIP, -1,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_PERMISSION_NOT_ALLOWED_CONFIRMATION,
         IDS_PERMISSIONS_WEB_INSTALL_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT,
         IDS_PERMISSIONS_PERMISSION_ALLOWED_ONCE_CONFIRMATION,
         IDS_PERMISSIONS_WEB_INSTALL_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT}}});

  auto messages = kMessageIds->find(request_type());
  if (messages != kMessageIds->end() && messages->second[type] != -1)
    return l10n_util::GetStringUTF16(messages->second[type]);

  return std::nullopt;
}

std::u16string PermissionRequest::GetMessageTextFragment() const {
  int message_id = 0;
  switch (request_type()) {
    case RequestType::kArSession:
      message_id = IDS_AR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCameraPanTiltZoom:
      message_id = IDS_MEDIA_CAPTURE_CAMERA_PAN_TILT_ZOOM_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCameraStream:
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY_PERMISSION_FRAGMENT;
      break;
    case RequestType::kCapturedSurfaceControl:
      message_id = IDS_CAPTURED_SURFACE_CONTROL_PERMISSION_FRAGMENT;
      break;
    case RequestType::kClipboard:
      message_id = IDS_CLIPBOARD_PERMISSION_FRAGMENT;
      break;
    case RequestType::kDiskQuota:
      message_id = IDS_REQUEST_QUOTA_PERMISSION_FRAGMENT;
      break;
    case RequestType::kFileSystemAccess:
      message_id = IDS_SITE_SETTINGS_TYPE_FILE_SYSTEM_ACCESS_WRITE;
      break;
    case RequestType::kGeolocation:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kHandTracking:
      message_id = IDS_HAND_TRACKING_PERMISSION_FRAGMENT;
      break;
    case RequestType::kIdleDetection:
      message_id = IDS_IDLE_DETECTION_PERMISSION_FRAGMENT;
      break;
    case RequestType::kKeyboardLock:
      message_id = IDS_KEYBOARD_LOCK_PERMISSIONS_FRAGMENT;
      break;
    case RequestType::kLocalFonts:
      message_id = IDS_FONT_ACCESS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMidiSysex:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
    case RequestType::kMultipleDownloads:
      message_id = IDS_MULTI_DOWNLOAD_PERMISSION_FRAGMENT;
      break;
    case RequestType::kNotifications:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
    case RequestType::kPointerLock:
      message_id = IDS_POINTER_LOCK_PERMISSIONS_FRAGMENT;
      break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case RequestType::kProtectedMediaIdentifier:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT;
      break;
#endif
    case RequestType::kRegisterProtocolHandler:
      // Handled by an override in `RegisterProtocolHandlerPermissionRequest`.
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
#if BUILDFLAG(IS_CHROMEOS)
    case RequestType::kSmartCard:
      // Handled by an override in `SmartCardPermissionRequest`.
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
#endif
    case RequestType::kStorageAccess:
    case RequestType::kTopLevelStorageAccess:
      message_id = IDS_STORAGE_ACCESS_PERMISSION_FRAGMENT;
      break;
    case RequestType::kVrSession:
      message_id = IDS_VR_PERMISSION_FRAGMENT;
      break;
    case RequestType::kWebAppInstallation:
      message_id = IDS_WEB_APP_INSTALLATION_PERMISSION_FRAGMENT;
      break;
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
    case RequestType::kWebPrinting:
      message_id = IDS_WEB_PRINTING_PERMISSION_FRAGMENT;
      break;
#endif
    case RequestType::kWindowManagement:
      message_id = IDS_WINDOW_MANAGEMENT_PERMISSION_FRAGMENT;
      break;
    case RequestType::kIdentityProvider:
      message_id = IDS_IDENTITY_PROVIDER_PERMISSION_FRAGMENT;
      break;
  }
  DCHECK_NE(0, message_id);
  return l10n_util::GetStringUTF16(message_id);
}
#endif

std::optional<std::u16string> PermissionRequest::GetAllowAlwaysText() const {
  return std::nullopt;
}

bool PermissionRequest::ShouldUseTwoOriginPrompt() const {
  return request_type() == RequestType::kStorageAccess;
}

void PermissionRequest::PermissionGranted(bool is_one_time) {
  std::move(permission_decided_callback_)
      .Run(CONTENT_SETTING_ALLOW, is_one_time,
           /*is_final_decision=*/true);
}

void PermissionRequest::PermissionDenied() {
  std::move(permission_decided_callback_)
      .Run(CONTENT_SETTING_BLOCK, /*is_one_time=*/false,
           /*is_final_decision=*/true);
}

void PermissionRequest::Cancelled(bool is_final_decision) {
  if (permission_decided_callback_) {
    permission_decided_callback_.Run(CONTENT_SETTING_DEFAULT,
                                     /*is_one_time=*/false, is_final_decision);
  }
}

void PermissionRequest::RequestFinished() {
  std::move(delete_callback_).Run();
}

PermissionRequestGestureType PermissionRequest::GetGestureType() const {
  return PermissionUtil::GetGestureType(data_.user_gesture);
}

const std::vector<std::string>&
PermissionRequest::GetRequestedAudioCaptureDeviceIds() const {
  return data_.requested_audio_capture_device_ids;
}

const std::vector<std::string>&
PermissionRequest::GetRequestedVideoCaptureDeviceIds() const {
  return data_.requested_video_capture_device_ids;
}

ContentSettingsType PermissionRequest::GetContentSettingsType() const {
  auto type = RequestTypeToContentSettingsType(request_type());
  if (type.has_value())
    return type.value();
  return ContentSettingsType::DEFAULT;
}

std::u16string PermissionRequest::GetPermissionNameTextFragment() const {
  int message_id = 0;
  switch (request_type()) {
    case RequestType::kCameraStream:
      message_id = IDS_CAMERA_PERMISSION_NAME_FRAGMENT;
      break;
    case RequestType::kGeolocation:
      message_id = IDS_GEOLOCATION_NAME_FRAGMENT;
      break;
    case RequestType::kMicStream:
      message_id = IDS_MICROPHONE_PERMISSION_NAME_FRAGMENT;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
  DCHECK_NE(0, message_id);
  return l10n_util::GetStringUTF16(message_id);
}

void PermissionRequest::SetEmbeddedPermissionElementInitiatedForTesting(
    bool embedded_permission_element_initiated) {
  data_.embedded_permission_element_initiated =
      embedded_permission_element_initiated;
}

}  // namespace permissions
