// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_AUTHORIZATION_WRAPPER_TEST_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_AUTHORIZATION_WRAPPER_TEST_H_

#import <AVFoundation/AVFoundation.h>

#include "base/callback.h"
#include "base/task/task_traits.h"
#include "chrome/browser/media/webrtc/media_authorization_wrapper_mac.h"

enum AuthStatus {
  kNotDetermined,
  kRestricted,
  kDenied,
  kAllowed,
};

class MediaAuthorizationWrapperTest
    : public system_media_permissions::MediaAuthorizationWrapper {
 public:
  MediaAuthorizationWrapperTest() = default;
  ~MediaAuthorizationWrapperTest() final = default;
  void SetMockMediaPermissionStatus(AuthStatus status);

  // MediaAuthorizationWrapper:
  NSInteger AuthorizationStatusForMediaType(NSString* media_type) override;
  void RequestAccessForMediaType(NSString* media_type,
                                 base::RepeatingClosure callback,
                                 const base::TaskTraits& traits) final {}

 private:
  AuthStatus permission_status_ = kNotDetermined;

  DISALLOW_COPY_AND_ASSIGN(MediaAuthorizationWrapperTest);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_AUTHORIZATION_WRAPPER_TEST_H_
