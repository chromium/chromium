// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_FRAME_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_FRAME_VIEW_WIN_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/non_client_view.h"

// A Windows app window frame view.
class AppWindowFrameViewWin : public views::NonClientFrameView {
  METADATA_HEADER(AppWindowFrameViewWin, views::NonClientFrameView)

 public:
  explicit AppWindowFrameViewWin(views::Widget* widget);
  AppWindowFrameViewWin(const AppWindowFrameViewWin&) = delete;
  AppWindowFrameViewWin& operator=(const AppWindowFrameViewWin&) = delete;
  ~AppWindowFrameViewWin() override;

  // The insets to the client area due to the frame.
  gfx::Insets GetFrameInsets() const;

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

  // views::View implementation.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  raw_ptr<views::Widget> widget_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   AppWindowFrameViewWin,
                   views::NonClientFrameView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, AppWindowFrameViewWin)

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_FRAME_VIEW_WIN_H_
