// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_NATIVE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_NATIVE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_frame_provider.h"

class BrowserFrameViewLayoutLinuxNative;

// A specialization of BrowserFrameViewLinux that is also able to
// render frame buttons using the native toolkit.
class BrowserFrameViewLinuxNative : public BrowserFrameViewLinux {
  METADATA_HEADER(BrowserFrameViewLinuxNative, BrowserFrameViewLinux)
 public:
  BrowserFrameViewLinuxNative(
      BrowserFrame* frame,
      BrowserView* browser_view,
      BrowserFrameViewLayoutLinuxNative* layout,
      std::unique_ptr<ui::NavButtonProvider> nav_button_provider);

  BrowserFrameViewLinuxNative(const BrowserFrameViewLinuxNative&) = delete;
  BrowserFrameViewLinuxNative& operator=(const BrowserFrameViewLinuxNative&) =
      delete;

  ~BrowserFrameViewLinuxNative() override;

  // BrowserFrameViewLinux:
  void Layout(PassKey) override;
  FrameButtonStyle GetFrameButtonStyle() const override;
  int GetTranslucentTopAreaHeight() const override;

 protected:
  // BrowserFrameViewLinux:
  float GetRestoredCornerRadiusDip() const override;
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
      ui::NavButtonProvider::FrameButtonDisplayType type);

  std::unique_ptr<ui::NavButtonProvider> nav_button_provider_;

  const raw_ptr<BrowserFrameViewLayoutLinuxNative> layout_;

  DrawFrameButtonParams cache_{0, false, false};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_NATIVE_H_
