// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_BORDER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_BORDER_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class BrowserView;
class BrowserWindowInterface;

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

// Controller that manages the contents sharing border widget visibility.
class ContentsBorderController : public content::WebContentsObserver {
 public:
  explicit ContentsBorderController(BrowserView* browser_view);
  ~ContentsBorderController() override;

  void AboutToBeDiscarded(content::WebContents* web_contents) override;

 private:
  void InitializeBorderWidget();
  void OnActiveTabChanged(BrowserWindowInterface* browser_window_interface);
  void OnTabCaptureChange(bool is_capturing,
                          std::optional<gfx::Rect> border_location);

  base::CallbackListSubscription active_tab_change_subscription_;
  base::CallbackListSubscription tab_capture_change_subscription_;
  raw_ptr<BrowserView> browser_view_ = nullptr;
  std::unique_ptr<views::Widget> capture_content_border_widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_BORDER_CONTROLLER_H_
