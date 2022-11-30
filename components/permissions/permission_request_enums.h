// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_ENUMS_H_

namespace permissions {

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
