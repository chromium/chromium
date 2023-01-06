// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_util.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::PermissionType;

namespace permissions {

namespace {
// Represents the possible methods of delegating permissions from main frames
// to child frames.
enum class PermissionDelegationMode {
  // Permissions from the main frame are delegated to child frames.
  // This is the default delegation mode for permissions. If a main frame was
  // granted a permission that is delegated, its child frames will inherit that
  // permission if allowed by the permissions policy.
  kDelegated,
  // Permissions from the main frame are not delegated to child frames.
  // An undelegated permission will only be granted to a child frame if the
  // child frame's origin was previously granted access to the permission when
  // in a main frame.
  kUndelegated,
  // Permission access is a function of both the requesting and embedding
  // origins.
  kDoubleKeyed,
};

PermissionDelegationMode GetPermissionDelegationMode(
    ContentSettingsType permission) {
  // TODO(crbug.com/987654): Generalize this to other "background permissions",
  // that is, permissions that can be used by a service worker. This includes
  // durable storage, background sync, etc.
  if (permission == ContentSettingsType::NOTIFICATIONS)
    return PermissionDelegationMode::kUndelegated;
  if (permission == ContentSettingsType::STORAGE_ACCESS ||
      permission == ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS) {
    return PermissionDelegationMode::kDoubleKeyed;
  }
  return PermissionDelegationMode::kDelegated;
}
}  // namespace

// The returned strings must match any Field Trial configs for the Permissions
// kill switch e.g. Permissions.Action.Geolocation etc..
std::string PermissionUtil::GetPermissionString(
    ContentSettingsType content_type) {
  PermissionType permission;
  bool success = PermissionUtil::GetPermissionType(content_type, &permission);
  DCHECK(success);

  return blink::GetPermissionString(permission);
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
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_FUCHSIA)
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
    case ContentSettingsType::CLIPBOARD_SANITIZED_WRITE:
      *out = PermissionType::CLIPBOARD_SANITIZED_WRITE;
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
    case ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS:
      *out = PermissionType::TOP_LEVEL_STORAGE_ACCESS;
      break;
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      *out = PermissionType::CAMERA_PAN_TILT_ZOOM;
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      *out = PermissionType::WINDOW_MANAGEMENT;
      break;
    case ContentSettingsType::LOCAL_FONTS:
      *out = PermissionType::LOCAL_FONTS;
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
  PermissionType permission;
  return PermissionUtil::GetPermissionType(type, &permission);
}

bool PermissionUtil::IsLowPriorityPermissionRequest(
    const PermissionRequest* request) {
  return request->request_type() == RequestType::kNotifications ||
         request->request_type() == RequestType::kGeolocation;
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

// Due to dependency issues, this method is duplicated in
// content/browser/permissions/permission_util.cc.
GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

#if BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // If `allow_universal_access_from_file_urls` flag is enabled, a file:/// can
  // change its url via history.pushState/replaceState to any other url,
  // including about:blank. To avoid user confusion we should always use a
  // visible url, in other words `GetLastCommittedURL`.
  if (web_contents->GetOrCreateWebPreferences()
          .allow_universal_access_from_file_urls &&
      render_frame_host->GetLastCommittedOrigin().GetURL().SchemeIsFile()) {
    return render_frame_host->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  }
#endif

  return render_frame_host->GetLastCommittedOrigin().GetURL();
}

ContentSettingsType PermissionUtil::PermissionTypeToContentSettingTypeSafe(
    PermissionType permission) {
  switch (permission) {
    case PermissionType::MIDI:
      return ContentSettingsType::MIDI;
    case PermissionType::MIDI_SYSEX:
      return ContentSettingsType::MIDI_SYSEX;
    case PermissionType::NOTIFICATIONS:
      return ContentSettingsType::NOTIFICATIONS;
    case PermissionType::GEOLOCATION:
      return ContentSettingsType::GEOLOCATION;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_FUCHSIA)
      return ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER;
#else
      break;
#endif
    case PermissionType::DURABLE_STORAGE:
      return ContentSettingsType::DURABLE_STORAGE;
    case PermissionType::AUDIO_CAPTURE:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case PermissionType::VIDEO_CAPTURE:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
    case PermissionType::BACKGROUND_SYNC:
      return ContentSettingsType::BACKGROUND_SYNC;
    case PermissionType::SENSORS:
      return ContentSettingsType::SENSORS;
    case PermissionType::ACCESSIBILITY_EVENTS:
      return ContentSettingsType::ACCESSIBILITY_EVENTS;
    case PermissionType::CLIPBOARD_READ_WRITE:
      return ContentSettingsType::CLIPBOARD_READ_WRITE;
    case PermissionType::CLIPBOARD_SANITIZED_WRITE:
      return ContentSettingsType::CLIPBOARD_SANITIZED_WRITE;
    case PermissionType::PAYMENT_HANDLER:
      return ContentSettingsType::PAYMENT_HANDLER;
    case PermissionType::BACKGROUND_FETCH:
      return ContentSettingsType::BACKGROUND_FETCH;
    case PermissionType::IDLE_DETECTION:
      return ContentSettingsType::IDLE_DETECTION;
    case PermissionType::PERIODIC_BACKGROUND_SYNC:
      return ContentSettingsType::PERIODIC_BACKGROUND_SYNC;
    case PermissionType::WAKE_LOCK_SCREEN:
      return ContentSettingsType::WAKE_LOCK_SCREEN;
    case PermissionType::WAKE_LOCK_SYSTEM:
      return ContentSettingsType::WAKE_LOCK_SYSTEM;
    case PermissionType::NFC:
      return ContentSettingsType::NFC;
    case PermissionType::VR:
      return ContentSettingsType::VR;
    case PermissionType::AR:
      return ContentSettingsType::AR;
    case PermissionType::STORAGE_ACCESS_GRANT:
      return ContentSettingsType::STORAGE_ACCESS;
    case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      return ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS;
    case PermissionType::CAMERA_PAN_TILT_ZOOM:
      return ContentSettingsType::CAMERA_PAN_TILT_ZOOM;
    case PermissionType::WINDOW_MANAGEMENT:
      return ContentSettingsType::WINDOW_MANAGEMENT;
    case PermissionType::LOCAL_FONTS:
      return ContentSettingsType::LOCAL_FONTS;
    case PermissionType::DISPLAY_CAPTURE:
      return ContentSettingsType::DISPLAY_CAPTURE;
    case PermissionType::NUM:
      break;
  }

  return ContentSettingsType::DEFAULT;
}

ContentSettingsType PermissionUtil::PermissionTypeToContentSettingType(
    PermissionType permission) {
  ContentSettingsType content_setting =
      PermissionTypeToContentSettingTypeSafe(permission);
  DCHECK_NE(content_setting, ContentSettingsType::DEFAULT)
      << "Unknown content setting for permission "
      << static_cast<int>(permission);
  return content_setting;
}

PermissionType PermissionUtil::ContentSettingTypeToPermissionType(
    ContentSettingsType permission) {
  PermissionType permissionType;
  bool success = PermissionUtil::GetPermissionType(permission, &permissionType);
  DCHECK(success);

  return permissionType;
}

ContentSetting PermissionUtil::PermissionStatusToContentSetting(
    blink::mojom::PermissionStatus status) {
  switch (status) {
    case blink::mojom::PermissionStatus::GRANTED:
      return CONTENT_SETTING_ALLOW;
    case blink::mojom::PermissionStatus::ASK:
      return CONTENT_SETTING_ASK;
    case blink::mojom::PermissionStatus::DENIED:
    default:
      return CONTENT_SETTING_BLOCK;
  }

  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

blink::mojom::PermissionStatus PermissionUtil::ContentSettingToPermissionStatus(
    ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return blink::mojom::PermissionStatus::GRANTED;
    case CONTENT_SETTING_BLOCK:
      return blink::mojom::PermissionStatus::DENIED;
    case CONTENT_SETTING_ASK:
      return blink::mojom::PermissionStatus::ASK;
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      break;
  }

  NOTREACHED();
  return blink::mojom::PermissionStatus::DENIED;
}

content::PermissionResult PermissionUtil::ToContentPermissionResult(
    PermissionResult result) {
  content::PermissionStatusSource source =
      (content::PermissionStatusSource)result.source;
  blink::mojom::PermissionStatus status =
      ContentSettingToPermissionStatus(result.content_setting);

  return content::PermissionResult(status, source);
}

PermissionResult PermissionUtil::ToPermissionResult(
    content::PermissionResult result) {
  PermissionStatusSource source = (PermissionStatusSource)result.source;
  ContentSetting setting = PermissionStatusToContentSetting(result.status);

  return PermissionResult(setting, source);
}

bool PermissionUtil::IsPermissionBlockedInPartition(
    ContentSettingsType permission,
    const GURL& requesting_origin,
    content::RenderProcessHost* render_process_host) {
  DCHECK(render_process_host);
  switch (GetPermissionDelegationMode(permission)) {
    case PermissionDelegationMode::kDelegated:
      return false;
    case PermissionDelegationMode::kDoubleKeyed:
      return false;
    case PermissionDelegationMode::kUndelegated:
      // TODO(crbug.com/1312218): This will create |requesting_origin|'s home
      // StoragePartition if it doesn't already exist. Given how
      // StoragePartitions are used today, this shouldn't actually be a
      // problem, but ideally we'd compare StoragePartitionConfigs.
      content::StoragePartition* requesting_home_partition =
          render_process_host->GetBrowserContext()->GetStoragePartitionForUrl(
              requesting_origin);
      return requesting_home_partition !=
             render_process_host->GetStoragePartition();
  }
}

GURL PermissionUtil::GetCanonicalOrigin(ContentSettingsType permission,
                                        const GURL& requesting_origin,
                                        const GURL& embedding_origin) {
  absl::optional<GURL> override_origin =
      PermissionsClient::Get()->OverrideCanonicalOrigin(requesting_origin,
                                                        embedding_origin);
  if (override_origin)
    return override_origin.value();

  switch (GetPermissionDelegationMode(permission)) {
    case PermissionDelegationMode::kDelegated:
      return embedding_origin;
    case PermissionDelegationMode::kDoubleKeyed:
    case PermissionDelegationMode::kUndelegated:
      return requesting_origin;
  }
}

bool PermissionUtil::HasUserGesture(PermissionPrompt::Delegate* delegate) {
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate->Requests();
  return std::any_of(
      requests.begin(), requests.end(),
      [](permissions::PermissionRequest* request) {
        return request->GetGestureType() ==
               permissions::PermissionRequestGestureType::GESTURE;
      });
}
}  // namespace permissions
