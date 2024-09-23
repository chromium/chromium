// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_context_base.h"
#include "content/public/browser/permission_request_description.h"

namespace permissions {

PermissionRequestData::PermissionRequestData(
    PermissionContextBase* context,
    const PermissionRequestID& id,
    const content::PermissionRequestDescription& request_description,
    const GURL& canonical_requesting_origin)
    : request_type(ContentSettingsTypeToRequestTypeIfExists(
          context->content_settings_type())),
      id(id),
      user_gesture(request_description.user_gesture),
      embedded_permission_element_initiated(
          request_description.embedded_permission_element_initiated),
      requesting_origin(canonical_requesting_origin),
      anchor_element_position(request_description.anchor_element_position),
      requested_audio_capture_device_ids(
          request_description.requested_audio_capture_device_ids),
      requested_video_capture_device_ids(
          request_description.requested_video_capture_device_ids) {}

PermissionRequestData::PermissionRequestData(PermissionContextBase* context,
                                             const PermissionRequestID& id,
                                             bool user_gesture,
                                             const GURL& requesting_origin,
                                             const GURL& embedding_origin)
    : request_type(ContentSettingsTypeToRequestTypeIfExists(
          context->content_settings_type())),
      id(id),
      user_gesture(user_gesture),
      embedded_permission_element_initiated(false),
      requesting_origin(requesting_origin),
      embedding_origin(embedding_origin) {}

PermissionRequestData::PermissionRequestData(RequestType request_type,
                                             bool user_gesture,
                                             const GURL& requesting_origin,
                                             const GURL& embedding_origin)
    : request_type(request_type),
      id(PermissionRequestID(
          content::GlobalRenderFrameHostId(0, 0),
          permissions::PermissionRequestID::RequestLocalId())),
      user_gesture(user_gesture),
      embedded_permission_element_initiated(false),
      requesting_origin(requesting_origin),
      embedding_origin(embedding_origin) {}

PermissionRequestData& PermissionRequestData::operator=(
    PermissionRequestData&&) = default;
PermissionRequestData::PermissionRequestData(PermissionRequestData&&) = default;

PermissionRequestData::~PermissionRequestData() = default;

}  // namespace permissions
