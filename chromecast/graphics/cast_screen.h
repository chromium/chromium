// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_SCREEN_H_
#define CHROMECAST_GRAPHICS_CAST_SCREEN_H_

#include <optional>

#include "chromecast/public/graphics_types.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"

namespace chromecast {
namespace shell {
class CastBrowserMainParts;
}  // namespace shell

// CastScreen is Chromecast's own minimal implementation of display::Screen.
// Right now, it almost exactly duplicates the behavior of aura's TestScreen
// class for necessary methods. The instantiation of CastScreen occurs in
// CastBrowserMainParts, where its ownership is assigned to CastBrowserProcess.
// To then subsequently access CastScreen, see CastBrowerProcess.
class CastScreen : public display::ScreenBase {
 public:
  CastScreen();

  CastScreen(const CastScreen&) = delete;
  CastScreen& operator=(const CastScreen&) = delete;

  ~CastScreen() override;

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;

  void OnDisplayChanged(int64_t display_id,
                        float scale_factor,
                        display::Display::Rotation rotation,
                        const gfx::Rect& bounds);

  // Temporarily override the primary display settings with new bounds, scale
  // factor, and rotation.
  // The original display settings are stashed for later retrieval.
  void OverridePrimaryDisplaySettings(const gfx::Rect& bounds,
                                      float scale_factor,
                                      display::Display::Rotation rotation);

  // Restore stashed display settings stored by OverridePrimaryDisplaySettings.
  // Returns true if stashed settings were applied. False if none were
  // available.
  bool RestorePrimaryDisplaySettings();

 private:
  std::optional<display::Display> stashed_display_settings_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_SCREEN_H_
