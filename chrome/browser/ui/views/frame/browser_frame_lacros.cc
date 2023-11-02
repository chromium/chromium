// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_lacros.h"

BrowserFrameLacros::BrowserFrameLacros(BrowserView* browser_view)
    : BrowserFrame(browser_view) {}

// In Lacros, windows that use 'custom frame' should draw their own
// frame header (ie client-side window decoration). This is the case of
// the lacros/chrome browser windows, for instance.
// On the other hand, windows like devtools do not use a "custom frame"
// and hence defer its frame header drawing to the host WindowManager
// (ie server-side window decoration).
bool BrowserFrameLacros::ShouldDrawFrameHeader() const {
  return UseCustomFrame();
}
