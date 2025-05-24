// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/permissions/permission_request.h"
#include "url/gurl.h"

namespace permissions {
enum class RequestType;

class MockPermissionRequest : public PermissionRequest {
 public:
  struct MockPermissionRequestState {
   public:
    MockPermissionRequestState();
    ~MockPermissionRequestState();

    base::WeakPtr<MockPermissionRequestState> GetWeakPtr();

    bool granted;
    bool cancelled;
    bool finished;
    RequestType request_type;

   private:
    base::WeakPtrFactory<MockPermissionRequestState> weak_factory_{this};
  };

  static constexpr const char* kDefaultOrigin = "https://www.google.com";

  explicit MockPermissionRequest(
      RequestType request_type,
      base::WeakPtr<MockPermissionRequestState> request_state = nullptr);
  MockPermissionRequest(
      const GURL& requesting_origin,
      RequestType request_type,
      base::WeakPtr<MockPermissionRequestState> request_state = nullptr);
  MockPermissionRequest(
      RequestType request_type,
      PermissionRequestGestureType gesture_type,
      base::WeakPtr<MockPermissionRequestState> request_state = nullptr);
  MockPermissionRequest(
      const GURL& requesting_origin,
      RequestType request_type,
      PermissionRequestGestureType gesture_type,
      base::WeakPtr<MockPermissionRequestState> request_state = nullptr);
  MockPermissionRequest(
      const GURL& requesting_origin,
      RequestType request_type,
      bool embedded_permission_element_initiated,
      base::WeakPtr<MockPermissionRequestState> request_state = nullptr);
  MockPermissionRequest(
      const GURL& requesting_origin,
      RequestType request_type,
      std::vector<std::string> requested_audio_capture_device_ids,
      std::vector<std::string> requested_video_capture_device_ids,
      base::WeakPtr<MockPermissionRequestState> request_state = nullptr);

  ~MockPermissionRequest() override;

  void RegisterOnPermissionDecidedCallback(base::OnceClosure callback);

  void PermissionDecided(
      ContentSetting result,
      bool is_one_time,
      bool is_final_decision,
      const permissions::PermissionRequestData& request_data);

  const std::vector<std::string>& GetRequestedAudioCaptureDeviceIds()
      const override;
  const std::vector<std::string>& GetRequestedVideoCaptureDeviceIds()
      const override;

  std::unique_ptr<MockPermissionRequest> CreateDuplicateRequest(
      base::WeakPtr<MockPermissionRequestState> request_state = nullptr) const;

 private:
  base::WeakPtr<MockPermissionRequestState> request_state_;

  base::OnceClosure on_permission_decided_;
  std::vector<std::string> requested_audio_capture_device_ids_;
  std::vector<std::string> requested_video_capture_device_ids_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
