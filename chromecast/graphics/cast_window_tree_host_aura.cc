// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_tree_host_aura.h"

#include "ui/aura/null_window_targeter.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace chromecast {

CastWindowTreeHostAura::CastWindowTreeHostAura(
    bool enable_input,
    ui::PlatformWindowInitProperties properties)
    : WindowTreeHostPlatform(std::move(properties)),
      enable_input_(enable_input) {
  if (!enable_input) {
    window()->SetEventTargeter(std::make_unique<aura::NullWindowTargeter>());
  }

  ui_event_source_ = UiEventSource::Create(this);
}

CastWindowTreeHostAura::~CastWindowTreeHostAura() {}

void CastWindowTreeHostAura::DispatchEvent(ui::Event* event) {
  if (!enable_input_) {
    return;
  }

  if (ui_event_source_ && event != nullptr &&
      !ui_event_source_->ShouldDispatchEvent(*event)) {
    // Filter out unnecessary events.
    return;
  }

  WindowTreeHostPlatform::DispatchEvent(event);
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
