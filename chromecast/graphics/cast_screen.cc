// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_screen.h"

#include <stdint.h>

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/display/display.h"

namespace chromecast {

CastScreen::~CastScreen() {
}

gfx::Point CastScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool CastScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow CastScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return gfx::NativeWindow();
}

display::Display CastScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

CastScreen::CastScreen() {
}

void CastScreen::OnDisplayChanged(int64_t display_id,
                                  float device_scale_factor,
                                  display::Display::Rotation rotation,
                                  const gfx::Rect& bounds) {
  display::Display display(display_id);
  display.SetScaleAndBounds(device_scale_factor, bounds);
  display.set_rotation(rotation);
  DVLOG(1) << __func__ << " " << display.ToString();
  ProcessDisplayChanged(display, true /* is_primary */);
}

void CastScreen::OverridePrimaryDisplaySettings(
    const gfx::Rect& bounds,
    float scale_factor,
    display::Display::Rotation rotation) {
  const auto primary_display = GetPrimaryDisplay();

  stashed_display_settings_ = primary_display;

  LOG(INFO) << "Stashing primary display (" << primary_display.id()
            << ") settings; device_scale_factor: "
            << stashed_display_settings_->device_scale_factor()
            << " rotation: " << stashed_display_settings_->RotationAsDegree()
            << " size (pixels): "
            << stashed_display_settings_->GetSizeInPixel().ToString();

  OnDisplayChanged(primary_display.id(), scale_factor, rotation, bounds);
}

bool CastScreen::RestorePrimaryDisplaySettings() {
  if (!stashed_display_settings_) {
    return false;
  }

  LOG(INFO)
      << "Restoring original primary display settings; device_scale_factor: "
      << stashed_display_settings_->device_scale_factor()
      << " rotation: " << stashed_display_settings_->RotationAsDegree()
      << " size (pixels): "
      << stashed_display_settings_->GetSizeInPixel().ToString();
  OnDisplayChanged(stashed_display_settings_->id(),
                   stashed_display_settings_->device_scale_factor(),
                   stashed_display_settings_->rotation(),
                   gfx::Rect(stashed_display_settings_->GetSizeInPixel()));
  stashed_display_settings_.reset();

  return true;
}

}  // namespace chromecast
