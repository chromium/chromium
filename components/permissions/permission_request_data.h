// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_DATA_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_DATA_H_

#include <optional>
#include <variant>

#include "base/values.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {
struct PermissionRequestDescription;
}

namespace permissions {

enum class GeolocationPromptType {
  kApproximateOrPrecise,
  kApproximateOnly,
  kUpgradeToPrecise
};

// Holds information about `permissions::PermissionRequest`
struct PermissionRequestData {
  PermissionRequestData(
      const PermissionRequestID& id,
      const content::PermissionRequestDescription& request_description,
      const GURL& canonical_requesting_origin,
      const GURL& embedding_origin = GURL(),
      int request_description_permission_index = 0);

  PermissionRequestData(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      const PermissionRequestID& id,
      bool user_gesture,
      const GURL& requesting_origin,
      const GURL& embedding_origin = GURL());

  PermissionRequestData(RequestType request_type,
                        bool user_gesture,
                        const GURL& requesting_origin,
                        const GURL& embedding_origin = GURL());

 private:
  PermissionRequestData(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      const PermissionRequestID& id,
      bool user_gesture,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      blink::mojom::EmbeddedPermissionRequestDescriptorPtr
          embedded_permission_request_descriptor,
      std::vector<std::string> requested_audio_capture_device_ids,
      std::vector<std::string> requested_video_capture_device_ids);

  PermissionRequestData(
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
      std::optional<GeolocationPromptType> geolocation_prompt_type);

 public:
  PermissionRequestData Clone() const;

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

  PermissionRequestData& WithGeolocationPromptType(GeolocationPromptType type) {
    geolocation_prompt_type = type;
    return *this;
  }

  bool IsEmbeddedPermissionElementInitiated() const {
    return !!embedded_permission_request_descriptor;
  }

  bool IsGeolocationElementInitiated() const {
    return embedded_permission_request_descriptor &&
           embedded_permission_request_descriptor->detail &&
           embedded_permission_request_descriptor->detail->is_geolocation();
  }

  bool IsEligibleForHeuristicAutoGrant() const {
    return base::FeatureList::IsEnabled(
               features::kPermissionHeuristicAutoGrant) &&
           embedded_permission_request_descriptor &&
           embedded_permission_request_descriptor->detail &&
           embedded_permission_request_descriptor->detail->is_geolocation() &&
           !embedded_permission_request_descriptor->detail->get_geolocation()
                ->autolocate;
  }

  std::optional<bool> GetGeolocationAutolocate() const {
    if (embedded_permission_request_descriptor &&
        embedded_permission_request_descriptor->detail &&
        embedded_permission_request_descriptor->detail->is_geolocation()) {
      return embedded_permission_request_descriptor->detail->get_geolocation()
          ->autolocate;
    }
    return std::nullopt;
  }

  std::optional<gfx::Rect> GetAnchorElementPosition() const {
    if (embedded_permission_request_descriptor) {
      return embedded_permission_request_descriptor->element_position;
    }
    return std::nullopt;
  }

  std::optional<GeolocationAccuracy> GetRequestedGeolocationAccuracy() const;

  // The request type if it exists.
  std::optional<RequestType> request_type;

  // The permission descriptor.
  blink::mojom::PermissionDescriptorPtr permission_descriptor;

  //  Uniquely identifier of particular permission request.
  PermissionRequestID id;

  // Indicates the request is initiated by a user gesture.
  bool user_gesture;

  // The origin on whose behalf this permission request is being made.
  GURL requesting_origin;

  // The origin of embedding frame (generally is the top level frame).
  GURL embedding_origin;

  // If not null, this request comes from an embedded permission element,
  // and this holds element-specific data.
  blink::mojom::EmbeddedPermissionRequestDescriptorPtr
      embedded_permission_request_descriptor;

  std::vector<std::string> requested_audio_capture_device_ids;
  std::vector<std::string> requested_video_capture_device_ids;

  std::optional<GeolocationPromptType> geolocation_prompt_type;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_DATA_H_
