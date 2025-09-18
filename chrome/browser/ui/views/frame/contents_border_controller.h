// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_BORDER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_BORDER_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"

class BrowserView;
class ContentsContainerView;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace views {
class WebView;
}

// General controller that manages the ContentsContainerViewBorderController for
// each ContentsContainerView.
class ContentsBorderController {
 public:
  explicit ContentsBorderController(BrowserView* browser_view);
  ~ContentsBorderController();

  // Manages the border for a single contents container view
  class ContentsContainerViewBorderController {
   public:
    explicit ContentsContainerViewBorderController(
        ContentsContainerView* contents_container_view,
        BrowserView* browser_view);
    ~ContentsContainerViewBorderController();

    void OnWebContentsAttached(views::WebView* web_view);
    void OnWebContentsDetached(views::WebView* web_view);

    void OnTabCaptureChange(bool is_capturing);
    void OnTabCaptureLocationChange(std::optional<gfx::Rect> border_location);

   private:
    void UpdateWebContentsSubscription(content::WebContents* web_contents);

    raw_ptr<ContentsContainerView> contents_container_view_ = nullptr;
    raw_ptr<BrowserView> browser_view_ = nullptr;
    base::CallbackListSubscription web_contents_attached_subscription_;
    base::CallbackListSubscription web_contents_detached_subscription_;
    base::CallbackListSubscription tab_capture_change_subscription_;
    base::CallbackListSubscription tab_capture_location_change_subscription_;
  };

 private:
  std::vector<std::unique_ptr<ContentsContainerViewBorderController>>
      border_controllers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_BORDER_CONTROLLER_H_
