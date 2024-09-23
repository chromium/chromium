// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_request.h"

#include "components/permissions/request_type.h"

namespace permissions {

MockPermissionRequest::MockPermissionRequest(RequestType request_type)
    : MockPermissionRequest(GURL(kDefaultOrigin),
                            request_type,
                            PermissionRequestGestureType::UNKNOWN) {}

MockPermissionRequest::MockPermissionRequest(const GURL& requesting_origin,
                                             RequestType request_type)
    : MockPermissionRequest(requesting_origin,
                            request_type,
                            PermissionRequestGestureType::UNKNOWN) {}

MockPermissionRequest::MockPermissionRequest(
    RequestType request_type,
    PermissionRequestGestureType gesture_type)
    : MockPermissionRequest(GURL(kDefaultOrigin), request_type, gesture_type) {}

MockPermissionRequest::MockPermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    PermissionRequestGestureType gesture_type)
    : PermissionRequest(
          requesting_origin,
          request_type,
          gesture_type == PermissionRequestGestureType::GESTURE,
          base::BindRepeating(&MockPermissionRequest::PermissionDecided,
                              base::Unretained(this)),
          base::BindOnce(&MockPermissionRequest::MarkFinished,
                         base::Unretained(this))),
      granted_(false),
      cancelled_(false),
      finished_(false) {}

MockPermissionRequest::MockPermissionRequest(
    RequestType request_type,
    bool embedded_permission_element_initiated)
    : MockPermissionRequest(request_type,
                            embedded_permission_element_initiated
                                ? PermissionRequestGestureType::GESTURE
                                : PermissionRequestGestureType::NO_GESTURE) {
  SetEmbeddedPermissionElementInitiatedForTesting(
      embedded_permission_element_initiated);
}

MockPermissionRequest::MockPermissionRequest(
    const GURL& requesting_origin,
    RequestType request_type,
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids)
    : MockPermissionRequest(requesting_origin, request_type) {
  requested_audio_capture_device_ids_ = requested_audio_capture_device_ids;
  requested_video_capture_device_ids_ = requested_video_capture_device_ids;
}

MockPermissionRequest::~MockPermissionRequest() {
  // TODO(crbug.com/40142352): `PermissionRequest` enforces that
  // `RequestFinished` is called before its destructor runs, but a lot of tests
  // were written assuming it doesn't, so we need to call it here. Clean up
  // lifetime in the tests and then remove this call to `RequestFinished`.
  if (!finished_)
    RequestFinished();
}

void MockPermissionRequest::RegisterOnPermissionDecidedCallback(
    base::OnceClosure callback) {
  on_permission_decided_ = std::move(callback);
}

void MockPermissionRequest::PermissionDecided(ContentSetting result,
                                              bool is_one_time,
                                              bool is_final_decision) {
  granted_ = result == CONTENT_SETTING_ALLOW;
  if (result == CONTENT_SETTING_DEFAULT) {
    cancelled_ = true;
  }

  if (on_permission_decided_) {
    std::move(on_permission_decided_).Run();
  }
}

void MockPermissionRequest::MarkFinished() {
  finished_ = true;
}

bool MockPermissionRequest::granted() {
  return granted_;
}

bool MockPermissionRequest::cancelled() {
  return cancelled_;
}

bool MockPermissionRequest::finished() {
  return finished_;
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
MockPermissionRequest::CreateDuplicateRequest() const {
  return std::make_unique<MockPermissionRequest>(
      requesting_origin(), request_type(), GetGestureType());
}

}  // namespace permissions
