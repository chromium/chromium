// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view_ash.h"

namespace chrome {

std::unique_ptr<BrowserFrameView> CreateBrowserFrameView(
    BrowserWidget* widget,
    BrowserView* browser_view) {
  if (browser_view->browser()->is_type_picture_in_picture()) {
    return std::make_unique<PictureInPictureBrowserFrameViewAsh>(widget,
                                                                 browser_view);
  }

  auto frame_view =
      std::make_unique<BrowserFrameViewChromeOS>(widget, browser_view);
  frame_view->Init();
  return frame_view;
}

}  // namespace chrome
