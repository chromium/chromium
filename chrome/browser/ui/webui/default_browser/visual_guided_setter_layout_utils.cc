// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_layout_utils.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "base/numerics/ranges.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"

namespace visual_guided_setter {

namespace {

// Minimum dimensions of the WebUI docking area bounding box (in physical
// pixels) required to dock the Settings window. If the docking area is smaller
// than this, we degrade the flow to floating.
constexpr int kMinAnchorWidthPx = 320;
constexpr int kMinAnchorHeightPx = 160;

// Layout constants in DIPs. These values were determined based on the visual
// alignment with the native Windows Settings app to match the UX spec.
constexpr int kHorizontalInsetDip = 61;
constexpr int kPreferredHeightDip = 220;
constexpr int kMinHeightDip = 180;
}  // namespace

bool IsAnchorLargeEnoughForDocking(const gfx::Rect& anchor_rect) {
  return anchor_rect.width() >= kMinAnchorWidthPx &&
         anchor_rect.height() >= kMinAnchorHeightPx;
}

gfx::Rect ComputeDockedSettingsRectFromAnchor(HWND chrome_hwnd,
                                              const gfx::Rect& anchor_rect_dip,
                                              const gfx::Rect& work_area_px,
                                              HWND settings_hwnd) {
  // 1. Convert anchor rect to physical screen pixels.
  gfx::Rect anchor_px = display::win::GetScreenWin()->DIPToScreenRect(
      chrome_hwnd, anchor_rect_dip);

  // 2. Compute target dimensions in physical pixels.
  int target_width_dip =
      std::max(0, anchor_rect_dip.width() - 2 * kHorizontalInsetDip);
  int target_height_dip = kPreferredHeightDip;
  gfx::Size target_size_px = display::win::GetScreenWin()->DIPToScreenSize(
      chrome_hwnd, gfx::Size(target_width_dip, target_height_dip));

  int min_height_px =
      display::win::GetScreenWin()
          ->DIPToScreenSize(chrome_hwnd, gfx::Size(0, kMinHeightDip))
          .height();
  int target_height_px =
      std::clamp(target_size_px.height(), min_height_px,
                 std::max(min_height_px, work_area_px.height()));
  int target_width_px = target_size_px.width();

  // 3. Compute top frame offset (titlebar + borders) so inner client top aligns
  // directly with anchor top (no padding).
  HMONITOR monitor = ::MonitorFromWindow(chrome_hwnd, MONITOR_DEFAULTTONEAREST);
  int top_frame_offset =
      display::win::GetScreenWin()->GetSystemMetricsForMonitor(monitor,
                                                               SM_CYCAPTION) +
      display::win::GetScreenWin()->GetSystemMetricsForMonitor(monitor,
                                                               SM_CYSIZEFRAME) +
      display::win::GetScreenWin()->GetSystemMetricsForMonitor(
          monitor, SM_CXPADDEDBORDER);
  if (settings_hwnd && ::IsWindow(settings_hwnd)) {
    RECT win_rect, client_rect;
    if (::GetWindowRect(settings_hwnd, &win_rect) &&
        ::GetClientRect(settings_hwnd, &client_rect)) {
      POINT pt = {client_rect.left, client_rect.top};
      if (::ClientToScreen(settings_hwnd, &pt)) {
        int measured = pt.y - win_rect.top;
        if (measured > 0) {
          top_frame_offset = measured;
        }
      }
    }
  }

  int target_x_px = anchor_px.x() + (anchor_px.width() - target_width_px) / 2;
  int target_y_px = anchor_px.y() - top_frame_offset;

  gfx::Rect target_px(target_x_px, target_y_px, target_width_px,
                      target_height_px);

  // 4. Clamp the final pixel rect to the work area.
  target_px.AdjustToFit(work_area_px);

  return target_px;
}

gfx::Point ComputeArrowStartPointFromAnchor(const gfx::Rect& anchor_rect) {
  return gfx::Point(anchor_rect.right(),
                    anchor_rect.y() + anchor_rect.height() / 2);
}

gfx::Point ComputeArrowEndPoint(HWND settings_hwnd,
                                const gfx::Rect& target_rect) {
  // Distance from the window's top down to the row the guidance indicates.
  constexpr int kTopPaddingDip = 164;
  const int top_padding_px =
      display::win::GetScreenWin()
          ->DIPToScreenSize(settings_hwnd, gfx::Size(0, kTopPaddingDip))
          .height();

  return gfx::Point(
      target_rect.right(),
      target_rect.y() + std::min(top_padding_px, target_rect.height()));
}

bool IsDpiCompatibleForDocking(HWND chrome_hwnd,
                               const gfx::Rect& target_screen_px) {
  if (!chrome_hwnd || !display::Screen::Get()) {
    return false;
  }

  // Passing a nullptr HWND instructs ScreenWin to evaluate scaling strictly
  // based on the monitor nearest to the physical rect.
  gfx::Rect target_dip =
      display::win::GetScreenWin()->ScreenToDIPRect(nullptr, target_screen_px);

  display::Display target_display =
      display::Screen::Get()->GetDisplayMatching(target_dip);

  float target_scale = target_display.device_scale_factor();
  float chrome_scale =
      display::win::GetScreenWin()->GetScaleFactorForHWND(chrome_hwnd);

  return base::IsApproximatelyEqual(target_scale, chrome_scale,
                                    std::numeric_limits<float>::epsilon());
}

}  // namespace visual_guided_setter
