// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_TAB_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_TAB_BROWSER_FRAME_H_

#include "chrome/browser/ui/views/frame/browser_frame.h"

class BrowserView;

// A browser frame for ARC custom tab that doesn't have a frame header.
class CustomTabBrowserFrame : public BrowserFrame {
 public:
  explicit CustomTabBrowserFrame(BrowserView* browser_view);

  // BrowserFrame
  bool ShouldDrawFrameHeader() const override;

 protected:
  ~CustomTabBrowserFrame() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CUSTOM_TAB_BROWSER_FRAME_H_
