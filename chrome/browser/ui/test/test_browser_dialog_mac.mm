// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog_mac.h"

#import <Cocoa/Cocoa.h>

namespace internal {

void TestBrowserDialogInteractiveSetUp() {
  NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
  [NSApp activateIgnoringOtherApps:YES];
}

}  // namespace internal
