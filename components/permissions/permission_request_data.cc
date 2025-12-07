// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_data.h"

#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"

namespace permissions {

PermissionRequestData::PermissionRequestData(
    PermissionContextBase* context,
    const PermissionRequestID& id,
    const content::PermissionRequestDescription& request_description,
    const GURL& canonical_requesting_origin,
    const GURL& canonical_embedding_origin,
    int request_description_permission_index)
    : request_type(ContentSettingsTypeToRequestTypeIfExists(
          context->content_settings_type())),
      id(id),
      user_gesture(request_description.user_gesture),
      requesting_origin(canonical_requesting_origin),
      embedding_origin(canonical_embedding_origin),
      embedded_permission_request_descriptor(
          request_description.embedded_permission_request_descriptor.Clone()),
      requested_audio_capture_device_ids(
          request_description.requested_audio_capture_device_ids),
      requested_video_capture_device_ids(
          request_description.requested_video_capture_device_ids) {
  resolver = context->CreatePermissionResolver(
      request_description.permissions[request_description_permission_index]);
}

PermissionRequestData::PermissionRequestData(
    std::unique_ptr<permissions::PermissionResolver> resolver,
    const PermissionRequestID& id,
    bool user_gesture,
    const GURL& requesting_origin,
    const GURL& embedding_origin)
    : request_type(resolver->GetRequestType()),
      resolver(std::move(resolver)),
      id(id),
      user_gesture(user_gesture),
      requesting_origin(requesting_origin),
      embedding_origin(embedding_origin) {}

PermissionRequestData::PermissionRequestData(
    std::unique_ptr<permissions::PermissionResolver> resolver,
    bool user_gesture,
    const GURL& requesting_origin,
    const GURL& embedding_origin)
    : PermissionRequestData(
          std::move(resolver),
          PermissionRequestID(
              content::GlobalRenderFrameHostId(0, 0),
              permissions::PermissionRequestID::RequestLocalId()),
          user_gesture,
          requesting_origin,
          embedding_origin) {}

PermissionRequestData& PermissionRequestData::operator=(
    PermissionRequestData&&) = default;
PermissionRequestData::PermissionRequestData(PermissionRequestData&&) = default;

PermissionRequestData::~PermissionRequestData() = default;

}  // namespace permissions
