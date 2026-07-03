// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_LAYOUT_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_LAYOUT_UTILS_H_

#include "base/win/windows_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace visual_guided_setter {

// Returns true if the anchor rect (in physical pixels) is large enough to
// accommodate the docked Settings window.
bool IsAnchorLargeEnoughForDocking(const gfx::Rect& anchor_rect);

// Computes the target bounds for the docked Settings window (in physical
// pixels) based on the WebUI anchor rect in DIPs, and clamps it to the
// monitor's work area. All physical coordinate mapping is handled by
// display::win::ScreenWin.
gfx::Rect ComputeDockedSettingsRectFromAnchor(HWND chrome_hwnd,
                                              const gfx::Rect& anchor_rect_dip,
                                              const gfx::Rect& work_area_px,
                                              HWND settings_hwnd = nullptr);

// Computes the starting point of the guidance arrow (in physical pixels,
// screen coordinates) relative to the WebUI anchor rect.
gfx::Point ComputeArrowStartPointFromAnchor(const gfx::Rect& anchor_rect);

// Computes the ending point of the guidance arrow (in physical pixels,
// screen coordinates) relative to the Settings window rect.
gfx::Point ComputeArrowEndPoint(const gfx::Rect& target_rect);

// Returns true if the target docking rect is on a monitor with the same DPI
// scaling as the Chrome window.
bool IsDpiCompatibleForDocking(HWND chrome_hwnd,
                               const gfx::Rect& target_screen);

}  // namespace visual_guided_setter

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_LAYOUT_UTILS_H_
