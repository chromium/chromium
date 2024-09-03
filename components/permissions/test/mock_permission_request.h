// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_

#include "components/permissions/permission_request.h"
#include "url/gurl.h"

namespace permissions {
enum class RequestType;

class MockPermissionRequest : public PermissionRequest {
 public:
  static constexpr const char* kDefaultOrigin = "https://www.google.com";

  explicit MockPermissionRequest(RequestType request_type);
  MockPermissionRequest(const GURL& requesting_origin,
                        RequestType request_type);
  MockPermissionRequest(RequestType request_type,
                        PermissionRequestGestureType gesture_type);
  MockPermissionRequest(const GURL& requesting_origin,
                        RequestType request_type,
                        PermissionRequestGestureType gesture_type);
  MockPermissionRequest(RequestType request_type,
                        bool embedded_permission_element_initiated);
  MockPermissionRequest(
      const GURL& requesting_origin,
      RequestType request_type,
      std::vector<std::string> requested_audio_capture_device_ids,
      std::vector<std::string> requested_video_capture_device_ids);

  ~MockPermissionRequest() override;

  void RegisterOnPermissionDecidedCallback(base::OnceClosure callback);

  void PermissionDecided(ContentSetting result,
                         bool is_one_time,
                         bool is_final_decision);
  void MarkFinished();

  bool granted();
  bool cancelled();
  bool finished();

  const std::vector<std::string>& GetRequestedAudioCaptureDeviceIds()
      const override;
  const std::vector<std::string>& GetRequestedVideoCaptureDeviceIds()
      const override;

  std::unique_ptr<MockPermissionRequest> CreateDuplicateRequest() const;

 private:
  bool granted_;
  bool cancelled_;
  bool finished_;

  base::OnceClosure on_permission_decided_;
  std::vector<std::string> requested_audio_capture_device_ids_;
  std::vector<std::string> requested_video_capture_device_ids_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
