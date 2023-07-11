// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/media_authorization_wrapper_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void MediaAuthorizationWrapperTest::SetMockMediaPermissionStatus(
    AVAuthorizationStatus status) {
  permission_status_ = status;
}

AVAuthorizationStatus
MediaAuthorizationWrapperTest::AuthorizationStatusForMediaType(
    NSString* media_type) {
  return permission_status_;
}
