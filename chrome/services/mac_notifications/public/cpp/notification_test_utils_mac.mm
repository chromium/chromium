// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/notification_test_utils_mac.h"

@implementation FakeUNNotification
@synthesize request = _request;
- (void)dealloc {
  [_request release];
  [super dealloc];
}
@end

@implementation FakeUNNotificationResponse
@synthesize notification = _notification;
@synthesize actionIdentifier = _actionIdentifier;
- (void)dealloc {
  [_notification release];
  [_actionIdentifier release];
  [super dealloc];
}
@end
