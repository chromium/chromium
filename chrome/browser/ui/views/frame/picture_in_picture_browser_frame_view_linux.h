// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_LINUX_H_

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "ui/linux/window_frame_provider.h"

class BrowserWidget;
class BrowserView;

namespace views {
class FrameBackground;
}  // namespace views

class PictureInPictureBrowserFrameViewLinux
    : public PictureInPictureBrowserFrameView {
 public:
  // Gets the shadow metrics (radius, offset, and number of shadows) even if
  // shadows are not drawn.
  static gfx::ShadowValues GetShadowValues();

  PictureInPictureBrowserFrameViewLinux(BrowserWidget* widget,
                                        BrowserView* browser_view);

  PictureInPictureBrowserFrameViewLinux(
      const PictureInPictureBrowserFrameViewLinux&) = delete;
  PictureInPictureBrowserFrameViewLinux& operator=(
      const PictureInPictureBrowserFrameViewLinux&) = delete;

  ~PictureInPictureBrowserFrameViewLinux() override;

  // BrowserFrameView:
  gfx::Insets RestoredMirroredFrameBorderInsets() const override;
  gfx::Insets GetInputInsets() const override;
  SkRRect GetRestoredClipRegion() const override;

  // views::FrameView:
  gfx::Rect GetNonDecoratedClientAreaBoundsInScreen() const override;

  // PictureInPictureBrowserFrameView:
  gfx::Rect GetHitRegion() const override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::WidgetObserver:
  void OnWidgetShowStateChanged(views::Widget* widget) override;

  // Returns whether a client-side shadow should be drawn for the window.
  bool ShouldDrawFrameShadow() const;

 protected:
  // PictureInPictureBrowserFrameView:
  gfx::Insets ResizeBorderInsets() const override;
  gfx::Insets FrameBorderInsets() const override;

 private:
  // Returns the frame border insets for the restored window state. These are
  // used even while the window is maximized, e.g. to convert between the
  // window geometry and bounds of the restored window.
  gfx::Insets RestoredFrameBorderInsets() const;

  // Returns whether the frame border and shadow are drawn, which is the case
  // unless the window is maximized or fullscreen.
  bool IsShowingFrame() const;

  // Used to draw window frame borders and shadow on Linux when GTK theme is
  // enabled, for restored and maximized/fullscreen states respectively.
  raw_ptr<ui::WindowFrameProvider> window_frame_provider_ = nullptr;
  raw_ptr<ui::WindowFrameProvider> maximized_window_frame_provider_ = nullptr;

  // Used to draw window frame borders and shadow on Linux when classic theme is
  // enabled.
  std::unique_ptr<views::FrameBackground> frame_background_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_LINUX_H_
