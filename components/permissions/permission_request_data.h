// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_DATA_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_DATA_H_

#include "components/permissions/permission_request_id.h"
#include "components/permissions/request_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {
struct PermissionRequestDescription;
}

namespace permissions {

class PermissionContextBase;

// Holds information about `permissions::PermissionRequest`
struct PermissionRequestData {
  PermissionRequestData(
      PermissionContextBase* context,
      const PermissionRequestID& id,
      const content::PermissionRequestDescription& request_description,
      const GURL& canonical_requesting_origin);

  PermissionRequestData(PermissionContextBase* context,
                        const PermissionRequestID& id,
                        bool user_gesture,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin = GURL());

  PermissionRequestData(RequestType request_type,
                        bool user_gesture,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin = GURL());

  PermissionRequestData& operator=(const PermissionRequestData&) = delete;
  PermissionRequestData(const PermissionRequestData&) = delete;

  PermissionRequestData& operator=(PermissionRequestData&&);
  PermissionRequestData(PermissionRequestData&&);

  ~PermissionRequestData();

  PermissionRequestData& WithRequestingOrigin(const GURL& origin) {
    requesting_origin = origin;
    return *this;
  }

  PermissionRequestData& WithEmbeddingOrigin(const GURL& origin) {
    embedding_origin = origin;
    return *this;
  }

  // The type of request.
  absl::optional<RequestType> request_type;

  //  Uniquely identifier of particular permission request.
  PermissionRequestID id;

  // Indicates the request is initiated by a user gesture.
  bool user_gesture;

  // Indicates the request is initiated from an embedded permission element.
  bool embedded_permission_element_initiated;

  // The origin on whose behalf this permission request is being made.
  GURL requesting_origin;

  // The origin of embedding frame (generally is the top level frame).
  GURL embedding_origin;

  // Anchor element position (in screen coordinates), gennerally when the
  // permission request is made from permission element. Used to calculate
  // position where the secondary prompt UI is expected to be shown.
  absl::optional<gfx::Rect> anchor_element_position;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_DATA_H_
