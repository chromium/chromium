// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_tab_browser_frame.h"

CustomTabBrowserFrame::CustomTabBrowserFrame(BrowserView* browser_view)
    : BrowserFrame(browser_view) {}

bool CustomTabBrowserFrame::ShouldDrawFrameHeader() const {
  return false;
}
