// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_TAB_PARAMS_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_TAB_PARAMS_H_

#include "ui/display/types/display_constants.h"

// Parameters for entering "tab fullscreen" mode.
struct FullscreenTabParams {
  // Sites with the Window Management permission may request fullscreen on a
  // particular display. In that case, `display_id` is the display's id;
  // otherwise, display::kInvalidDisplayId indicates no display is specified.
  int64_t display_id = display::kInvalidDisplayId;

  // Prefer that the bottom navigation bar be shown when in fullscreen
  // mode on devices with overlay navigation bars.
  bool prefers_navigation_bar = false;

  // Prefer that the status bar be shown when in fullscreen mode on devices with
  // overlay navigation bars.
  bool prefers_status_bar = false;

  bool operator==(const FullscreenTabParams& rhs) const {
    return display_id == rhs.display_id &&
           prefers_navigation_bar == rhs.prefers_navigation_bar &&
           prefers_status_bar == rhs.prefers_status_bar;
  }
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_TAB_PARAMS_H_
