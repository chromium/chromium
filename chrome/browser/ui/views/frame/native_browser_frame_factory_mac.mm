// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#import "chrome/browser/ui/views/frame/browser_frame_mac.h"

NativeBrowserFrame* NativeBrowserFrameFactory::Create(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new BrowserFrameMac(browser_frame, browser_view);
}
