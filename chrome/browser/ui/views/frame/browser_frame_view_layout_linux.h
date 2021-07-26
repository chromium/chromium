// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_H_

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

// A specialization of OpaqueBrowserFrameViewLayout that takes into account
// extra padding added by client side shadows.
// TODO(https://crbug.com/650494): Implement this stub class.
class BrowserFrameViewLayoutLinux : public OpaqueBrowserFrameViewLayout {
 public:
  BrowserFrameViewLayoutLinux();

  BrowserFrameViewLayoutLinux(const BrowserFrameViewLayoutLinux&) = delete;
  BrowserFrameViewLayoutLinux& operator=(const BrowserFrameViewLayoutLinux&) =
      delete;

  ~BrowserFrameViewLayoutLinux() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_H_
