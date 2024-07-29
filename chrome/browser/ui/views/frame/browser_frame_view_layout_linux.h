// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

class BrowserFrameViewLinux;

// A specialization of OpaqueBrowserFrameViewLayout that takes into account
// extra padding added by client side shadows.
class BrowserFrameViewLayoutLinux : public OpaqueBrowserFrameViewLayout {
 public:
  BrowserFrameViewLayoutLinux();

  BrowserFrameViewLayoutLinux(const BrowserFrameViewLayoutLinux&) = delete;
  BrowserFrameViewLayoutLinux& operator=(const BrowserFrameViewLayoutLinux&) =
      delete;

  ~BrowserFrameViewLayoutLinux() override;

  gfx::Insets RestoredMirroredFrameBorderInsets() const;

  gfx::Insets GetInputInsets() const;

  void set_view(BrowserFrameViewLinux* view) { view_ = view; }

 protected:
  // OpaqueBrowserFrameViewLayout:
  int CaptionButtonY(views::FrameButton button_id,
                     bool restored) const override;
  gfx::Insets RestoredFrameBorderInsets() const override;
  gfx::Insets RestoredFrameEdgeInsets() const override;
  int NonClientExtraTopThickness() const override;

 private:
  raw_ptr<BrowserFrameViewLinux> view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LAYOUT_LINUX_H_
