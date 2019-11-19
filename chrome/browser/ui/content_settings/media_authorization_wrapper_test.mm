// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/media_authorization_wrapper_test.h"

void MediaAuthorizationWrapperTest::SetMockMediaPermissionStatus(
    AuthStatus status) {
  permission_status_ = status;
}

NSInteger MediaAuthorizationWrapperTest::AuthorizationStatusForMediaType(
    NSString* media_type) {
  return static_cast<NSInteger>(permission_status_);
}
