// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_GLASS_APP_WINDOW_FRAME_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_GLASS_APP_WINDOW_FRAME_VIEW_WIN_H_

#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/window/non_client_view.h"

// A glass style app window frame view.
class GlassAppWindowFrameViewWin : public views::NonClientFrameView {
 public:
  static const char kViewClassName[];

  explicit GlassAppWindowFrameViewWin(views::Widget* widget);
  ~GlassAppWindowFrameViewWin() override;

  // The insets to the client area due to the glass frame.
  gfx::Insets GetGlassInsets() const;

  // Additional insets to the client area.  |monitor| is the monitor this
  // window is on.  Normally that would be determined from the HWND, but
  // during WM_NCCALCSIZE Windows does not return the correct monitor for the
  // HWND, so it must be passed in explicitly.
  gfx::Insets GetClientAreaInsets(HMONITOR monitor) const;

 private:
  // views::NonClientFrameView implementation.
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  // views::View implementation.
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(GlassAppWindowFrameViewWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_GLASS_APP_WINDOW_FRAME_VIEW_WIN_H_
