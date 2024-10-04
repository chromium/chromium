// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_
#define COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_

#include <optional>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "printing/buildflags/buildflags.h"

namespace gfx {
struct VectorIcon;
}

namespace permissions {

// The type of the request that will be seen by the user. Values are only
// defined on the platforms where they are used and should be kept alphabetized.
enum class RequestType {
  kArSession,
#if !BUILDFLAG(IS_ANDROID)
  kCameraPanTiltZoom,
#endif
  kCameraStream,
#if !BUILDFLAG(IS_ANDROID)
  kCapturedSurfaceControl,
#endif
  kClipboard,
  kTopLevelStorageAccess,
  kDiskQuota,
#if !BUILDFLAG(IS_ANDROID)
  kFileSystemAccess,
#endif
  kGeolocation,
  kHandTracking,
  kIdentityProvider,
  kIdleDetection,
#if !BUILDFLAG(IS_ANDROID)
  kLocalFonts,
#endif
  kMicStream,
  kMidiSysex,
  kMultipleDownloads,
#if BUILDFLAG(IS_ANDROID)
  kNfcDevice,
#endif
  kNotifications,
#if !BUILDFLAG(IS_ANDROID)
  kKeyboardLock,
  kPointerLock,
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  kProtectedMediaIdentifier,
#endif
#if !BUILDFLAG(IS_ANDROID)
  kRegisterProtocolHandler,
#endif
#if BUILDFLAG(IS_CHROMEOS)
  kSmartCard,
#endif
  kStorageAccess,
  kVrSession,
#if !BUILDFLAG(IS_ANDROID)
  kWebAppInstallation,
#endif
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  kWebPrinting,
#endif
#if !BUILDFLAG(IS_ANDROID)
  kWindowManagement,
  kMaxValue = kWindowManagement
#else
  kMaxValue = kVrSession
#endif
};

#if BUILDFLAG(IS_ANDROID)
// On Android, icons are represented with an IDR_ identifier.
using IconId = int;
#else
// On desktop, we use a vector icon.
typedef const gfx::VectorIcon& IconId;
#endif

bool IsRequestablePermissionType(ContentSettingsType content_settings_type);

std::optional<RequestType> ContentSettingsTypeToRequestTypeIfExists(
    ContentSettingsType content_settings_type);

RequestType ContentSettingsTypeToRequestType(
    ContentSettingsType content_settings_type);

std::optional<ContentSettingsType> RequestTypeToContentSettingsType(
    RequestType request_type);

// Returns whether confirmation chips can be displayed
bool IsConfirmationChipSupported(RequestType for_request_type);

// Returns the icon to display.
IconId GetIconId(RequestType type);

#if !BUILDFLAG(IS_ANDROID)
// Returns the blocked icon to display.
IconId GetBlockedIconId(RequestType type);
#endif

// Returns a unique human-readable string that can be used in dictionaries that
// are keyed by the RequestType.
const char* PermissionKeyForRequestType(permissions::RequestType request_type);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_
