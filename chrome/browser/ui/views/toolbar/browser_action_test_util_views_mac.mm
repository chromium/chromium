// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/browser_action_test_util.h"

#include <AppKit/AppKit.h>

#import "base/mac/scoped_nsobject.h"
#import "ui/base/test/windowed_nsnotification_observer.h"

bool BrowserActionTestUtil::WaitForPopup() {
  NSWindow* window = [GetPopupNativeView().GetNativeNSView() window];
  if (!window)
    return false;

  if ([window isKeyWindow])
    return true;

  base::scoped_nsobject<WindowedNSNotificationObserver> waiter(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidBecomeKeyNotification
                       object:window]);

  BOOL notification_observed = [waiter wait];
  return notification_observed && [window isKeyWindow];
}
