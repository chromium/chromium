// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_test_helper.h"

#include <AppKit/AppKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/test/windowed_nsnotification_observer.h"

void ExtensionActionTestHelper::WaitForPopup() {
  NSWindow* window = GetPopupNativeView().GetNativeNSView().window;
  ASSERT_TRUE(window);

  if (!window.keyWindow) {
    WindowedNSNotificationObserver* waiter =
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowDidBecomeKeyNotification
                         object:window];
    BOOL notification_observed = [waiter wait];
    ASSERT_TRUE(notification_observed);
  }

  ASSERT_TRUE(window.keyWindow);
}
