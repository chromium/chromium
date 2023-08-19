// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_bubble_views_browsertest_mac.h"

#import <Cocoa/Cocoa.h>

namespace test {

float GetNativeWindowAlphaValue(gfx::NativeWindow window) {
  return window.GetNativeNSWindow().alphaValue;
}

}  // namespace test
