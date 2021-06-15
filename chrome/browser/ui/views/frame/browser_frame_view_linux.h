// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_

#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

// A specialization of OpaqueBrowserFrameView that is also able to
// render client side decorations (shadow, border, and rounded corners).
// TODO(https://crbug.com/650494): Implement this stub class.
class BrowserFrameViewLinux : public OpaqueBrowserFrameView {
 public:
  BrowserFrameViewLinux(BrowserFrame* frame,
                        BrowserView* browser_view,
                        BrowserFrameViewLayoutLinux* layout);

  BrowserFrameViewLinux(const BrowserFrameViewLinux&) = delete;
  BrowserFrameViewLinux& operator=(const BrowserFrameViewLinux&) = delete;

  ~BrowserFrameViewLinux() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_
