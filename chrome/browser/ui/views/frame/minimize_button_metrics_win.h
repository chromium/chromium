// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MINIMIZE_BUTTON_METRICS_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MINIMIZE_BUTTON_METRICS_WIN_H_

#include <windows.h>

// Class that implements obtaining the X coordinate of the native minimize
// button for the native frame on Windows.
// This is a separate class because obtaining it is somewhat tricky and this
// code is shared between BrowserDesktopWindowTreeHostWin and BrowserFrameWin.
class MinimizeButtonMetrics {
 public:
  MinimizeButtonMetrics();

  MinimizeButtonMetrics(const MinimizeButtonMetrics&) = delete;
  MinimizeButtonMetrics& operator=(const MinimizeButtonMetrics&) = delete;

  ~MinimizeButtonMetrics();

  void Init(HWND hwnd);

  // Obtain the X offset of the native minimize button. Since Windows can lie
  // to us if we call this at the wrong moment, this might come from a cached
  // value rather than read when called.
  int GetMinimizeButtonOffsetX() const;

  // Must be called when hwnd_ is activated to update the minimize button
  // position cache.
  void OnHWNDActivated();

  // Must be called when WM_DPICHANGED message is received.
  void OnDpiChanged();

 private:
  // Gets the value for GetMinimizeButtonOffsetX(), caching if found.
  int GetAndCacheMinimizeButtonOffsetX() const;

  int GetButtonBoundsPositionOffset(const RECT& button_bounds,
                                    const RECT& window_bounds) const;

  int GetMinimizeButtonOffsetForWindow() const;

  HWND hwnd_ = nullptr;

  // Cached offset of the minimize button. If RTL this is the location of the
  // minimize button, if LTR this is the offset from the right edge of the
  // client area to the minimize button.
  mutable int cached_minimize_button_x_delta_ =
      last_cached_minimize_button_x_delta_;

  // Static cache of `cached_minimize_button_x_delta_`.
  static int last_cached_minimize_button_x_delta_;

  // Static cache of offset value representing the difference between
  // DWMWA_CAPTION_BUTTON_BOUNDS and WM_GETTITLEBARINFOEX
  static int button_bounds_position_offset_;

  // Has OnHWNDActivated() been invoked?
  bool was_activated_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MINIMIZE_BUTTON_METRICS_WIN_H_
