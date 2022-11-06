// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"

namespace chrome {

std::unique_ptr<BrowserNonClientFrameView> CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
  if (browser_view->GetIsPictureInPictureType()) {
    return std::make_unique<PictureInPictureBrowserFrameView>(frame,
                                                              browser_view);
  }
  return std::make_unique<BrowserNonClientFrameViewMac>(frame, browser_view);
}

}  // namespace chrome
