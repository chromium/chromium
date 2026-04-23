// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_data.h"

#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"

namespace permissions {

PermissionRequestData::PermissionRequestData(
    const PermissionRequestID& id,
    const content::PermissionRequestDescription& request_description,
    const GURL& canonical_requesting_origin,
    const GURL& canonical_embedding_origin,
    int request_description_permission_index)
    : PermissionRequestData(
          request_description.permissions[request_description_permission_index],
          id,
          request_description.user_gesture,
          canonical_requesting_origin,
          canonical_embedding_origin,
          request_description.embedded_permission_request_descriptor.Clone(),
          request_description.requested_audio_capture_device_ids,
          request_description.requested_video_capture_device_ids) {}

PermissionRequestData::PermissionRequestData(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const PermissionRequestID& id,
    bool user_gesture,
    const GURL& requesting_origin,
    const GURL& embedding_origin)
    : PermissionRequestData(permission_descriptor,
                            id,
                            user_gesture,
                            requesting_origin,
                            embedding_origin,
                            nullptr,
                            {},
                            {}) {}

PermissionRequestData::PermissionRequestData(RequestType request_type,
                                             bool user_gesture,
                                             const GURL& requesting_origin,
                                             const GURL& embedding_origin)
    : PermissionRequestData(
          request_type,
          nullptr,
          PermissionRequestID(
              content::GlobalRenderFrameHostId(0, 0),
              permissions::PermissionRequestID::RequestLocalId()),
          user_gesture,
          requesting_origin,
          embedding_origin,
          nullptr,
          {},
          {},
          std::nullopt) {}

PermissionRequestData::PermissionRequestData(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const PermissionRequestID& id,
    bool user_gesture,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    blink::mojom::EmbeddedPermissionRequestDescriptorPtr
        embedded_permission_request_descriptor,
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids)
    : PermissionRequestData(
          permission_descriptor
              ? ContentSettingsTypeToRequestTypeIfExists(
                    PermissionUtil::PermissionTypeToContentSettingsType(
                        blink::PermissionDescriptorToPermissionType(
                            permission_descriptor)))
              : std::nullopt,
          permission_descriptor.Clone(),
          id,
          user_gesture,
          requesting_origin,
          embedding_origin,
          std::move(embedded_permission_request_descriptor),
          std::move(requested_audio_capture_device_ids),
          std::move(requested_video_capture_device_ids),
          std::nullopt) {}

PermissionRequestData::PermissionRequestData(
    std::optional<RequestType> request_type,
    blink::mojom::PermissionDescriptorPtr permission_descriptor,
    const PermissionRequestID& id,
    bool user_gesture,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    blink::mojom::EmbeddedPermissionRequestDescriptorPtr
        embedded_permission_request_descriptor,
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids,
    std::optional<GeolocationPromptType> geolocation_prompt_type)
    : request_type(std::move(request_type)),
      permission_descriptor(std::move(permission_descriptor)),
      id(id),
      user_gesture(user_gesture),
      requesting_origin(requesting_origin),
      embedding_origin(embedding_origin),
      embedded_permission_request_descriptor(
          std::move(embedded_permission_request_descriptor)),
      requested_audio_capture_device_ids(
          std::move(requested_audio_capture_device_ids)),
      requested_video_capture_device_ids(
          std::move(requested_video_capture_device_ids)),
      geolocation_prompt_type(std::move(geolocation_prompt_type)) {}

PermissionRequestData PermissionRequestData::Clone() const {
  return PermissionRequestData(
      request_type, permission_descriptor.Clone(), id, user_gesture,
      requesting_origin, embedding_origin,
      embedded_permission_request_descriptor.Clone(),
      requested_audio_capture_device_ids, requested_video_capture_device_ids,
      geolocation_prompt_type);
}

PermissionRequestData& PermissionRequestData::operator=(
    PermissionRequestData&&) = default;
PermissionRequestData::PermissionRequestData(PermissionRequestData&&) = default;

PermissionRequestData::~PermissionRequestData() = default;

std::optional<GeolocationAccuracy>
PermissionRequestData::GetRequestedGeolocationAccuracy() const {
  if (!permission_descriptor) {
    return std::nullopt;
  }

  switch (permission_descriptor->name) {
    case blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE:
      return GeolocationAccuracy::kApproximate;
    case blink::mojom::PermissionName::GEOLOCATION:
      return GeolocationAccuracy::kPrecise;
    default:
      return std::nullopt;
  }
}

}  // namespace permissions
