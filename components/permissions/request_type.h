// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_
#define COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

enum class ContentSettingsType;

namespace gfx {
struct VectorIcon;
}

namespace permissions {

// The type of the request that will be seen by the user. Values are only
// defined on the platforms where they are used and should be kept alphabetized.
enum class RequestType {
  kAccessibilityEvents,
  kArSession,
#if !BUILDFLAG(IS_ANDROID)
  kCameraPanTiltZoom,
#endif
  kCameraStream,
  kClipboard,
  kDiskQuota,
#if !BUILDFLAG(IS_ANDROID)
  kLocalFonts,
#endif
  kGeolocation,
  kIdleDetection,
  kMicStream,
  kMidiSysex,
  kMultipleDownloads,
#if BUILDFLAG(IS_ANDROID)
  kNfcDevice,
#endif
  kNotifications,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  kProtectedMediaIdentifier,
#endif
#if !BUILDFLAG(IS_ANDROID)
  kRegisterProtocolHandler,
  kSecurityAttestation,
#endif
  kStorageAccess,
#if !BUILDFLAG(IS_ANDROID)
  kU2fApiRequest,
#endif
  kVrSession,
#if !BUILDFLAG(IS_ANDROID)
  kWindowPlacement,
  kMaxValue = kWindowPlacement
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

RequestType ContentSettingsTypeToRequestType(
    ContentSettingsType content_settings_type);

absl::optional<ContentSettingsType> RequestTypeToContentSettingsType(
    RequestType request_type);

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
