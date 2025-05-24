// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_request.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
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
    : PermissionRequest(
          std::make_unique<PermissionRequestData>(
              std::make_unique<ContentSettingPermissionResolver>(request_type),
              /*user_gesture=*/gesture_type ==
                  PermissionRequestGestureType::GESTURE,
              requesting_origin),
          base::BindRepeating(&MockPermissionRequest::PermissionDecided,
                              base::Unretained(this)),
          base::DoNothing()),
      request_state_(request_state) {
  if (request_state) {
    request_state->finished = false;
    request_state->cancelled = false;
    request_state->granted = false;
    request_state->request_type = request_type;
  }
}

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

void MockPermissionRequest::RegisterOnPermissionDecidedCallback(
    base::OnceClosure callback) {
  on_permission_decided_ = std::move(callback);
}

void MockPermissionRequest::PermissionDecided(
    ContentSetting result,
    bool is_one_time,
    bool is_final_decision,
    const permissions::PermissionRequestData& request_data) {
  if (request_state_) {
    request_state_->granted = result == CONTENT_SETTING_ALLOW;

    if (result == CONTENT_SETTING_DEFAULT) {
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
