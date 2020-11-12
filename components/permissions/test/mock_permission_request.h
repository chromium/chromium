// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
#define COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/permissions/permission_request.h"
#include "url/gurl.h"

namespace permissions {

class MockPermissionRequest : public PermissionRequest {
 public:
  MockPermissionRequest();
  explicit MockPermissionRequest(const std::string& text);
  MockPermissionRequest(const std::string& text,
                        PermissionRequestType request_type,
                        PermissionRequestGestureType gesture_type);
  MockPermissionRequest(const std::string& text,
                        PermissionRequestType request_type,
                        const GURL& url);
  MockPermissionRequest(const std::string& text,
                        const std::string& accept_label,
                        const std::string& deny_label);
  MockPermissionRequest(const std::string& text,
                        ContentSettingsType content_settings_type_);

  ~MockPermissionRequest() override;

  IconId GetIconId() const override;
#if defined(OS_ANDROID)
  base::string16 GetMessageText() const override;
#endif
  base::string16 GetMessageTextFragment() const override;
  GURL GetOrigin() const override;

  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  PermissionRequestType GetPermissionRequestType() const override;
  PermissionRequestGestureType GetGestureType() const override;
  ContentSettingsType GetContentSettingsType() const override;

  bool granted();
  bool cancelled();
  bool finished();

 private:
  MockPermissionRequest(const std::string& text,
                        const std::string& accept_label,
                        const std::string& deny_label,
                        const GURL& url,
                        PermissionRequestType request_type,
                        PermissionRequestGestureType gesture_type,
                        ContentSettingsType content_settings_type);
  bool granted_;
  bool cancelled_;
  bool finished_;
  PermissionRequestType request_type_;
  PermissionRequestGestureType gesture_type_;
  ContentSettingsType content_settings_type_;

  base::string16 text_;
  base::string16 accept_label_;
  base::string16 deny_label_;
  GURL origin_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_MOCK_PERMISSION_REQUEST_H_
