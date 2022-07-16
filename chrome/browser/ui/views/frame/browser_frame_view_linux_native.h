// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_NATIVE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_NATIVE_H_

#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "ui/views/linux_ui/nav_button_provider.h"
#include "ui/views/linux_ui/window_frame_provider.h"

// A specialization of BrowserFrameViewLinux that is also able to
// render frame buttons using the native toolkit.
class BrowserFrameViewLinuxNative : public BrowserFrameViewLinux {
 public:
  BrowserFrameViewLinuxNative(
      BrowserFrame* frame,
      BrowserView* browser_view,
      BrowserFrameViewLayoutLinux* layout,
      std::unique_ptr<views::NavButtonProvider> nav_button_provider,
      views::WindowFrameProvider* window_frame_provider);

  BrowserFrameViewLinuxNative(const BrowserFrameViewLinuxNative&) = delete;
  BrowserFrameViewLinuxNative& operator=(const BrowserFrameViewLinuxNative&) =
      delete;

  ~BrowserFrameViewLinuxNative() override;

 protected:
  // BrowserFrameViewLinux:
  float GetRestoredCornerRadiusDip() const override;

  // OpaqueBrowserFrameView:
  void Layout() override;
  FrameButtonStyle GetFrameButtonStyle() const override;

  // views::View:
  void PaintRestoredFrameBorder(gfx::Canvas* canvas) const override;

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
  views::Button* GetButtonFromDisplayType(
      views::NavButtonProvider::FrameButtonDisplayType type);

  std::unique_ptr<views::NavButtonProvider> nav_button_provider_;

  views::WindowFrameProvider* const window_frame_provider_;

  DrawFrameButtonParams cache_{0, false, false};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_NATIVE_H_
