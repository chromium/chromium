// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_tree_host_aura.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace chromecast {

CastWindowTreeHostAura::CastWindowTreeHostAura(
    ui::PlatformWindowInitProperties properties)
    : WindowTreeHostPlatform(std::move(properties)) {
  // base::Unretained(this) is safe because |ui_event_source_| is owned by
  // |this| and will be destroyed before |this|.
  ui_event_source_ = UiEventSource::Create(
      this,
      base::BindRepeating(&CastWindowTreeHostAura::OnWindowSizeChanged,
                          base::Unretained(this)),
      base::BindRepeating(&CastWindowTreeHostAura::OnTransformChanged,
                          base::Unretained(this)));
}

CastWindowTreeHostAura::~CastWindowTreeHostAura() {}

void CastWindowTreeHostAura::DispatchEvent(ui::Event* event) {
  if (ui_event_source_ && event != nullptr &&
      !ui_event_source_->ShouldDispatchEvent(*event)) {
    // Filter out unnecessary events.
    return;
  }

  WindowTreeHostPlatform::DispatchEvent(event);
}

void CastWindowTreeHostAura::OnBoundsChanged(const BoundsChange& change) {
  WindowTreeHostPlatform::OnBoundsChanged(change);
  UpdateDisplayMetrics();
}

void CastWindowTreeHostAura::OnWindowSizeChanged(const gfx::Size& new_size) {
  gfx::Size physical_size = GetBoundsInPixels().size();
  const gfx::Transform& transform =
      window() ? window()->transform() : gfx::Transform();
  bool has_portrait_transform =
      (std::abs(transform.rc(0, 1)) > 0.5f &&
       std::abs(transform.rc(1, 0)) > 0.5f) ||
      (last_starboard_transform_.has_value() &&
       std::abs(last_starboard_transform_->rc(0, 1)) > 0.5f &&
       std::abs(last_starboard_transform_->rc(1, 0)) > 0.5f);
  bool is_orientation_swap =
      has_portrait_transform && (new_size.width() == physical_size.height() &&
                                 new_size.height() == physical_size.width());

  if (!is_orientation_swap && new_size != physical_size) {
    SetBoundsInPixels(gfx::Rect(new_size));
  }

  UpdateDisplayMetrics();
}

void CastWindowTreeHostAura::OnTransformChanged(
    const gfx::Transform& transform) {
  last_starboard_transform_ = transform;
  if (!window()) {
    return;
  }
  if (!rotation_supported_) {
    SetRootTransform(gfx::Transform());
    UpdateDisplayMetrics();
    return;
  }

  float dsf = device_scale_factor();
  if (dsf <= 0.0f) {
    dsf = 1.0f;
  }
  gfx::Transform dip_transform = transform;
  dip_transform.set_rc(0, 3, transform.rc(0, 3) / dsf);
  dip_transform.set_rc(1, 3, transform.rc(1, 3) / dsf);
  SetRootTransform(dip_transform);

  UpdateDisplayMetrics();
}

void CastWindowTreeHostAura::SetDefaultRootTransform(
    const gfx::Transform& transform) {
  default_root_transform_ = transform;
}

void CastWindowTreeHostAura::SetRotationSupported(bool supported) {
  if (rotation_supported_ == supported) {
    return;
  }
  rotation_supported_ = supported;
  if (!window()) {
    return;
  }
  if (!rotation_supported_) {
    SetRootTransform(gfx::Transform());
    UpdateDisplayMetrics();
  } else if (last_starboard_transform_.has_value()) {
    OnTransformChanged(*last_starboard_transform_);
  } else {
    SetRootTransform(default_root_transform_);
    UpdateDisplayMetrics();
  }
}

void CastWindowTreeHostAura::UpdateDisplayMetrics() {
  display::Screen* screen = display::Screen::Get();
  if (!screen) {
    return;
  }
  const display::Display& display = screen->GetPrimaryDisplay();
  const gfx::Transform& transform = window()->transform();

  display::Display::Rotation rotation = display::Display::ROTATE_0;
  bool is_portrait = (std::abs(transform.rc(0, 1)) > 0.5f &&
                      std::abs(transform.rc(1, 0)) > 0.5f);
  if (is_portrait) {
    rotation = (transform.rc(0, 1) < 0.0f) ? display::Display::ROTATE_90
                                           : display::Display::ROTATE_270;
  } else if (transform.rc(0, 0) < 0.0f) {
    rotation = display::Display::ROTATE_180;
  }

  gfx::Size physical_size = GetBoundsInPixels().size();
  gfx::Size display_size =
      is_portrait
          ? gfx::Size(std::min(physical_size.width(), physical_size.height()),
                      std::max(physical_size.width(), physical_size.height()))
          : gfx::Size(std::max(physical_size.width(), physical_size.height()),
                      std::min(physical_size.width(), physical_size.height()));

  display::Display updated_display(display.id());
  updated_display.SetScaleAndBounds(display.device_scale_factor(),
                                    gfx::Rect(display_size));
  updated_display.set_rotation(rotation);
  auto* screen_base = static_cast<display::ScreenBase*>(screen);
  if (screen_base->display_list().FindDisplayById(updated_display.id()) ==
      screen_base->display_list().displays().end()) {
    screen_base->display_list().AddDisplay(updated_display,
                                           display::DisplayList::Type::PRIMARY);
  } else {
    screen_base->display_list().UpdateDisplay(
        updated_display, display::DisplayList::Type::PRIMARY);
  }
}

gfx::Rect CastWindowTreeHostAura::GetTransformedRootWindowBoundsFromPixelSize(
    const gfx::Size& size_in_pixels) const {
  gfx::RectF new_bounds =
      GetInverseRootTransform().MapRect(gfx::RectF(gfx::Rect(size_in_pixels)));

  // Root window origin will be (0,0) except during bounds changes.
  // Set to exactly zero to avoid rounding issues.
  return gfx::Rect(gfx::ToCeiledSize(new_bounds.size()));
}

}  // namespace chromecast
