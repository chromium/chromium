// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_

namespace permissions {

// Used for UMA to record the types of permission prompts shown.
// When updating, you also need to update:
//   1) The PermissionRequestType enum in tools/metrics/histograms/enums.xml.
//   2) The PermissionRequestTypes suffix list in
//      tools/metrics/histograms/histograms.xml.
//   3) GetPermissionRequestString in
//      chrome/browser/permissions/permission_uma_util.cc.
//
// The usual rules of updating UMA values applies to this enum:
// - don't remove values
// - only ever add values at the end
enum class PermissionRequestType {
  UNKNOWN = 0,
  MULTIPLE = 1,
  // UNUSED_PERMISSION = 2,
  QUOTA = 3,
  DOWNLOAD = 4,
  // MEDIA_STREAM = 5,
  REGISTER_PROTOCOL_HANDLER = 6,
  PERMISSION_GEOLOCATION = 7,
  PERMISSION_MIDI_SYSEX = 8,
  PERMISSION_NOTIFICATIONS = 9,
  PERMISSION_PROTECTED_MEDIA_IDENTIFIER = 10,
  // PERMISSION_PUSH_MESSAGING = 11,
  PERMISSION_FLASH = 12,
  PERMISSION_MEDIASTREAM_MIC = 13,
  PERMISSION_MEDIASTREAM_CAMERA = 14,
  PERMISSION_ACCESSIBILITY_EVENTS = 15,
  // PERMISSION_CLIPBOARD_READ = 16, // Replaced by
  // PERMISSION_CLIPBOARD_READ_WRITE in M81.
  PERMISSION_SECURITY_KEY_ATTESTATION = 17,
  PERMISSION_PAYMENT_HANDLER = 18,
  PERMISSION_NFC = 19,
  PERMISSION_CLIPBOARD_READ_WRITE = 20,
  PERMISSION_VR = 21,
  PERMISSION_AR = 22,
  PERMISSION_STORAGE_ACCESS = 23,
  PERMISSION_CAMERA_PAN_TILT_ZOOM = 24,
  PERMISSION_WINDOW_PLACEMENT = 25,
  PERMISSION_FONT_ACCESS = 26,
  PERMISSION_IDLE_DETECTION = 27,
  // NUM must be the last value in the enum.
  NUM
};

// Used for UMA to record whether a gesture was associated with the request. For
// simplicity not all request types track whether a gesture is associated with
// it or not, for these types of requests metrics are not recorded.
enum class PermissionRequestGestureType {
  UNKNOWN,
  GESTURE,
  NO_GESTURE,
  // NUM must be the last value in the enum.
  NUM
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_
