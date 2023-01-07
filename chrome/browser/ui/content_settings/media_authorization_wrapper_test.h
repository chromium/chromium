// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_AUTHORIZATION_WRAPPER_TEST_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_AUTHORIZATION_WRAPPER_TEST_H_

#import <AVFoundation/AVFoundation.h>

#include "base/functional/callback.h"
#include "chrome/browser/media/webrtc/media_authorization_wrapper_mac.h"

enum AuthStatus {
  kNotDetermined,
  kRestricted,
  kDenied,
  kAllowed,
};

class MediaAuthorizationWrapperTest final
    : public system_media_permissions::MediaAuthorizationWrapper {
 public:
  MediaAuthorizationWrapperTest() = default;

  MediaAuthorizationWrapperTest(const MediaAuthorizationWrapperTest&) = delete;
  MediaAuthorizationWrapperTest& operator=(
      const MediaAuthorizationWrapperTest&) = delete;

  ~MediaAuthorizationWrapperTest() override = default;
  void SetMockMediaPermissionStatus(AuthStatus status);

  // MediaAuthorizationWrapper:
  NSInteger AuthorizationStatusForMediaType(NSString* media_type) override;
  void RequestAccessForMediaType(NSString* media_type,
                                 base::OnceClosure callback) override {}

 private:
  AuthStatus permission_status_ = kNotDetermined;
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_AUTHORIZATION_WRAPPER_TEST_H_
