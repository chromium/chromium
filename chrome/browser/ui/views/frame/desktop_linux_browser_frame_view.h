// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_H_

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

// A specialization of OpaqueBrowserFrameView that is also able to
// render frame buttons using GTK.
class DesktopLinuxBrowserFrameView : public OpaqueBrowserFrameView {
 public:
  DesktopLinuxBrowserFrameView(
      BrowserFrame* frame,
      BrowserView* browser_view,
      OpaqueBrowserFrameViewLayout* layout,
      std::unique_ptr<views::NavButtonProvider> nav_button_provider);
  ~DesktopLinuxBrowserFrameView() override;

 protected:
  // OpaqueBrowserFrameView:
  void Layout() override;

 private:
  struct DrawFrameButtonParams {
    bool operator==(const DrawFrameButtonParams& other) const;

    int top_area_height;
    bool maximized;
    bool active;
  };

  // Redraws the image resources associated with the minimize, maximize,
  // restore, and close buttons.
  virtual void MaybeUpdateCachedFrameButtonImages();

  // Returns one of |{minimize,maximize,restore,close}_button_|
  // corresponding to |type|.
  views::ImageButton* GetButtonFromDisplayType(
      chrome::FrameButtonDisplayType type);

  std::unique_ptr<views::NavButtonProvider> nav_button_provider_;

  DrawFrameButtonParams cache_{0, false, false};

  DISALLOW_COPY_AND_ASSIGN(DesktopLinuxBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_DESKTOP_LINUX_BROWSER_FRAME_VIEW_H_
