// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/win/windows_version.h"
#include "dwmapi.h"
#include "ui/base/win/shell.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point.h"

namespace {

// These constants were determined by manually adding various offsets
// until the identity switcher was placed at the same location as before.
// When a new or updated OS version is released, a new constant may need
// to be added to this list and GetDefaultButtonBoundsOffset() is updated.
const int kWin7ButtonBoundsPositionOffset = 1;
const int kWin8ButtonBoundsPositionOffset = 10;
const int kWin10ButtonBoundsPositionOffset = 6;
const int kInvalidOffset = static_cast<int>(0x80000000);

using base::win::GetVersion;
using display::win::ScreenWin;

int GetDefaultButtonBoundsOffset() {
  if (GetVersion() >= base::win::Version::WIN10)
    return kWin10ButtonBoundsPositionOffset;
  if (GetVersion() >= base::win::Version::WIN8)
    return kWin8ButtonBoundsPositionOffset;
  return kWin7ButtonBoundsPositionOffset;
}

}  // namespace

// static
int MinimizeButtonMetrics::last_cached_minimize_button_x_delta_ = 0;

// static
int MinimizeButtonMetrics::button_bounds_position_offset_ = kInvalidOffset;

MinimizeButtonMetrics::MinimizeButtonMetrics()
    : hwnd_(nullptr),
      cached_minimize_button_x_delta_(last_cached_minimize_button_x_delta_),
      was_activated_(false) {
}

MinimizeButtonMetrics::~MinimizeButtonMetrics() {
}

void MinimizeButtonMetrics::Init(HWND hwnd) {
  DCHECK(!hwnd_);
  hwnd_ = hwnd;
}

void MinimizeButtonMetrics::OnHWNDActivated() {
  was_activated_ = true;
  // NOTE: we don't cache here as it seems only after the activate is the value
  // correct.
}

void MinimizeButtonMetrics::OnDpiChanged() {
  // This ensures that the next time GetMinimizeButtonOffsetX() is called, it
  // will be recalculated, given the new scale factor.
  cached_minimize_button_x_delta_ = 0;
}

// This function attempts to calculate the odd and varying difference
// between the results of DwmGetWindowAttribute with the
// DWMWA_CAPTION_BUTTON_BOUNDS flag and the information from the
// WM_GETTITLEBARINFOEX message. It will return an empirically determined
// offset until the window has been activated and the message returns
// valid rectangles.
int MinimizeButtonMetrics::GetButtonBoundsPositionOffset(
    const RECT& button_bounds,
    const RECT& window_bounds) const {
  if (button_bounds_position_offset_ == kInvalidOffset) {
    if (!was_activated_ || !IsWindowVisible(hwnd_))
      return GetDefaultButtonBoundsOffset();
    TITLEBARINFOEX info = {0};
    info.cbSize = sizeof(info);
    SendMessage(hwnd_, WM_GETTITLEBARINFOEX, 0,
                reinterpret_cast<LPARAM>(&info));
    if (info.rgrect[2].right == info.rgrect[2].left ||
        (info.rgstate[2] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN |
                            STATE_SYSTEM_UNAVAILABLE)))
      return GetDefaultButtonBoundsOffset();
    button_bounds_position_offset_ =
        info.rgrect[2].left - (button_bounds.left + window_bounds.left);
  }
  return button_bounds_position_offset_;
}

int MinimizeButtonMetrics::GetMinimizeButtonOffsetForWindow() const {
  bool dwm_button_pos = false;
  POINT minimize_button_corner = {0};
  RECT button_bounds = {0};
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd_, DWMWA_CAPTION_BUTTON_BOUNDS,
                                      &button_bounds, sizeof(button_bounds)))) {
    if (button_bounds.left != button_bounds.right) {
      // This converts the button coordinate into screen coordinates
      // thus, ensuring that the identity switcher is placed in the
      // same location as before. An additional constant is added because
      // there is a difference between the caption button bounds and
      // the values obtained through WM_GETTITLEBARINFOEX. This difference
      // varies between OS versions, and no metric describing this difference
      // has been located.
      RECT window_bounds = {0};
      if (GetWindowRect(hwnd_, &window_bounds)) {
        int offset =
            GetButtonBoundsPositionOffset(button_bounds, window_bounds);
        minimize_button_corner = {
            button_bounds.left + window_bounds.left + offset, 0};
        dwm_button_pos = true;
      }
    }
  }
  if (!dwm_button_pos) {
    // Fallback to using the message for the titlebar info only if the above
    // code fails. It can fail if DWM is disabled globally or only for the
    // given HWND. The WM_GETTITLEBARINFOEX message can fail if we are not
    // active/visible. By fail we get a location of 0; the return status
    // code is always the same and similarly the state never seems to change
    // (titlebar_info.rgstate).
    TITLEBARINFOEX titlebar_info = {0};
    titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
    SendMessage(hwnd_, WM_GETTITLEBARINFOEX, 0,
                reinterpret_cast<LPARAM>(&titlebar_info));

    // Under DWM WM_GETTITLEBARINFOEX won't return the right thing until after
    // WM_NCACTIVATE (maybe it returns classic values?). In an attempt to
    // return a consistant value we cache the last value across instances and
    // use it until we get the activate.
    if (titlebar_info.rgrect[2].left == titlebar_info.rgrect[2].right ||
        (titlebar_info.rgstate[2] &
         (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN |
          STATE_SYSTEM_UNAVAILABLE)))
      return 0;
    minimize_button_corner = {titlebar_info.rgrect[2].left, 0};
  }

  // WM_GETTITLEBARINFOEX returns rects in screen coordinates in pixels.
  // DWMNA_CAPTION_BUTTON_BOUNDS is in window (not client) coordinates,
  // but it has been converted to screen coordinates above. We need to
  // convert the minimize button corner offset to DIP before returning it.
  MapWindowPoints(HWND_DESKTOP, hwnd_, &minimize_button_corner, 1);
  gfx::Point pixel_point = {minimize_button_corner.x, 0};
  gfx::Point dip_point = ScreenWin::ClientToDIPPoint(hwnd_, pixel_point);
  return dip_point.x();
}

int MinimizeButtonMetrics::GetMinimizeButtonOffsetX() const {
  // Under DWM WM_GETTITLEBARINFOEX won't return the right thing until after
  // WM_NCACTIVATE (maybe it returns classic values?). In an attempt to return a
  // consistant value we cache the last value across instances and use it until
  // we get the activate.
  if (was_activated_ || !ui::win::IsAeroGlassEnabled() ||
      cached_minimize_button_x_delta_ == 0) {
    const int minimize_button_offset = GetAndCacheMinimizeButtonOffsetX();
    if (minimize_button_offset > 0)
      return minimize_button_offset;
  }

  // If we fail to get the minimize button offset via the WM_GETTITLEBARINFOEX
  // message then calculate and return this via the
  // cached_minimize_button_x_delta_ member value. Please see
  // CacheMinimizeButtonDelta() for more details.
  DCHECK(cached_minimize_button_x_delta_);

  if (base::i18n::IsRTL())
    return cached_minimize_button_x_delta_;

  RECT client_rect = {0};
  GetClientRect(hwnd_, &client_rect);
  return client_rect.right - cached_minimize_button_x_delta_;
}

int MinimizeButtonMetrics::GetAndCacheMinimizeButtonOffsetX() const {
  const int minimize_button_offset = GetMinimizeButtonOffsetForWindow();
  if (minimize_button_offset <= 0)
    return 0;

  if (base::i18n::IsRTL()) {
    cached_minimize_button_x_delta_ = minimize_button_offset;
  } else {
    RECT client_rect = {0};
    GetClientRect(hwnd_, &client_rect);
    cached_minimize_button_x_delta_ =
        client_rect.right - minimize_button_offset;
  }
  last_cached_minimize_button_x_delta_ = cached_minimize_button_x_delta_;
  return minimize_button_offset;
}
