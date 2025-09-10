// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/linux/window_button_order_observer.h"

namespace ui {
class LinuxUi;
}  // namespace ui

// A specialization of OpaqueBrowserFrameView that is also able to
// render client side decorations (shadow, border, and rounded corners).
class BrowserFrameViewLinux : public OpaqueBrowserFrameView,
                              public ui::WindowButtonOrderObserver {
  METADATA_HEADER(BrowserFrameViewLinux, OpaqueBrowserFrameView)

 public:
  BrowserFrameViewLinux(BrowserFrame* frame,
                        BrowserView* browser_view,
                        BrowserFrameViewLayoutLinux* layout);

  BrowserFrameViewLinux(const BrowserFrameViewLinux&) = delete;
  BrowserFrameViewLinux& operator=(const BrowserFrameViewLinux&) = delete;

  ~BrowserFrameViewLinux() override;

  BrowserFrameViewLayoutLinux* layout() { return layout_; }

  // BrowserNonClientFrameView:
  gfx::Insets RestoredMirroredFrameBorderInsets() const override;
  gfx::Insets GetInputInsets() const override;
  SkRRect GetRestoredClipRegion() const override;
  int GetTranslucentTopAreaHeight() const override;

  // Gets the shadow metrics (radius, offset, and number of shadows).  This will
  // always return shadow values, even if shadows are not actually drawn.
  // `active` indicates if the shadow will be drawn on a focused browser window.
  static gfx::ShadowValues GetShadowValues(bool active);

 protected:
  // OpaqueBrowserFrameView:
  void PaintRestoredFrameBorder(gfx::Canvas* canvas) const override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  bool ShouldDrawRestoredFrameShadow() const override;

  // ui::WindowButtonOrderObserver:
  void OnWindowButtonOrderingChange() override;

  // views::NonClientFrameView:
  int NonClientHitTest(const gfx::Point& point) override;

  // Gets the radius of the top corners when the window is restored.  The
  // returned value is in DIPs.  The result will be 0 if rounded corners are
  // disabled (eg. if the compositor doesn't support translucency.)
  virtual float GetRestoredCornerRadiusDip() const;

 private:
  const raw_ptr<BrowserFrameViewLayoutLinux> layout_;

  base::ScopedObservation<ui::LinuxUi, ui::WindowButtonOrderObserver>
      window_button_order_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_VIEW_LINUX_H_
