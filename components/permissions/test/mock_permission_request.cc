// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_request.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"

namespace permissions {

MockPermissionRequest::MockPermissionRequestState::
    MockPermissionRequestState() = default;
MockPermissionRequest::MockPermissionRequestState::
    ~MockPermissionRequestState() = default;

base::WeakPtr<MockPermissionRequest::MockPermissionRequestState>
MockPermissionRequest::MockPermissionRequestState::GetWeakPtr() {
  return MockPermissionRequest::MockPermissionRequestState::weak_factory_
      .GetWeakPtr();
}

MockPermissionRequest::MockPermissionRequest(
    RequestType request_type,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : MockPermissionRequest(GURL(kDefaultOrigin),
                            request_type,
                            PermissionRequestGestureType::UNKNOWN,
                            request_state) {}

MockPermissionRequest::MockPermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : MockPermissionRequest(requesting_origin,
                            request_type,
                            PermissionRequestGestureType::UNKNOWN,
                            request_state) {}

MockPermissionRequest::MockPermissionRequest(
    RequestType request_type,
    PermissionRequestGestureType gesture_type,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : MockPermissionRequest(GURL(kDefaultOrigin),
                            request_type,
                            gesture_type,
                            request_state) {}

MockPermissionRequest::MockPermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    PermissionRequestGestureType gesture_type,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : MockPermissionRequest(requesting_origin,
                            request_type,
                            gesture_type,
                            std::nullopt,
                            request_state) {}

MockPermissionRequest::MockPermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    PermissionRequestGestureType gesture_type,
    std::optional<GeolocationPromptType> geolocation_prompt_type,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : MockPermissionRequest(
          [&] {
            auto data = std::make_unique<PermissionRequestData>(
                request_type,
                /*user_gesture=*/gesture_type ==
                    PermissionRequestGestureType::GESTURE,
                requesting_origin);
            // The geolocation prompt type is set to the default
            // kApproximateOrPrecise if not specified. Outside of tests, the
            // permission request manager is responsible for setting the
            // geolocation prompt type.
            if (geolocation_prompt_type.has_value()) {
              data->WithGeolocationPromptType(geolocation_prompt_type.value());
            } else if (base::FeatureList::IsEnabled(
                           content_settings::features::
                               kApproximateGeolocationPermission)) {
              data->WithGeolocationPromptType(
                  GeolocationPromptType::kApproximateOrPrecise);
            }
            return data;
          }(),
          request_state) {}

MockPermissionRequest::MockPermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    bool embedded_permission_element_initiated,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : MockPermissionRequest(request_type,
                            embedded_permission_element_initiated
                                ? PermissionRequestGestureType::GESTURE
                                : PermissionRequestGestureType::NO_GESTURE,
                            request_state) {
  SetEmbeddedPermissionElementInitiatedForTesting(
      embedded_permission_element_initiated);
}

MockPermissionRequest::MockPermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : MockPermissionRequest(requesting_origin, request_type, request_state) {
  requested_audio_capture_device_ids_ = requested_audio_capture_device_ids;
  requested_video_capture_device_ids_ = requested_video_capture_device_ids;
}

MockPermissionRequest::~MockPermissionRequest() {
  if (request_state_) {
    request_state_->finished = true;
  }
}

MockPermissionRequest::MockPermissionRequest(
    std::unique_ptr<PermissionRequestData> request_data,
    base::WeakPtr<MockPermissionRequestState> request_state)
    : PermissionRequest(
          std::move(request_data),
          base::BindRepeating(&MockPermissionRequest::PermissionDecided,
                              base::Unretained(this)),
          base::DoNothing()),
      request_state_(request_state) {
  if (request_state_) {
    request_state_->finished = false;
    request_state_->cancelled = false;
    request_state_->granted = false;
    request_state_->request_type = request_type();
  }
}

void MockPermissionRequest::RegisterOnPermissionDecidedCallback(
    base::OnceClosure callback) {
  on_permission_decided_ = std::move(callback);
}

void MockPermissionRequest::PermissionDecided(
    const permissions::PermissionPromptDecision& decision,
    const permissions::PermissionRequestData& request_data) {
  if (request_state_) {
    request_state_->granted =
        (decision.overall_decision == PermissionDecision::kAllow) ||
        (decision.overall_decision == PermissionDecision::kAllowThisTime);

    if (decision.overall_decision == PermissionDecision::kNone) {
      request_state_->cancelled = true;
    }
  }
  if (on_permission_decided_) {
    std::move(on_permission_decided_).Run();
  }
}

const std::vector<std::string>&
MockPermissionRequest::GetRequestedAudioCaptureDeviceIds() const {
  return requested_audio_capture_device_ids_;
}

const std::vector<std::string>&
MockPermissionRequest::GetRequestedVideoCaptureDeviceIds() const {
  return requested_video_capture_device_ids_;
}

std::unique_ptr<MockPermissionRequest>
MockPermissionRequest::CreateDuplicateRequest(
    base::WeakPtr<MockPermissionRequestState> request_state) const {
  return std::make_unique<MockPermissionRequest>(
      requesting_origin(), request_type(), GetGestureType(), request_state);
}

}  // namespace permissions
