// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_tree_host_aura.h"

#include "ui/aura/null_window_targeter.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace chromecast {

CastWindowTreeHostAura::CastWindowTreeHostAura(
    bool enable_input,
    ui::PlatformWindowInitProperties properties,
    bool use_external_frame_control)
    : WindowTreeHostPlatform(std::move(properties),
                             nullptr,
                             nullptr,
                             use_external_frame_control),
      enable_input_(enable_input) {
  if (!enable_input)
    window()->SetEventTargeter(std::make_unique<aura::NullWindowTargeter>());
}

CastWindowTreeHostAura::~CastWindowTreeHostAura() {}

void CastWindowTreeHostAura::DispatchEvent(ui::Event* event) {
  if (!enable_input_) {
    return;
  }

  WindowTreeHostPlatform::DispatchEvent(event);
}

gfx::Rect CastWindowTreeHostAura::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& size_in_pixels) const {
  gfx::RectF new_bounds = gfx::RectF(gfx::Rect(size_in_pixels));
  GetInverseRootTransform().TransformRect(&new_bounds);

  // Root window origin will be (0,0) except during bounds changes.
  // Set to exactly zero to avoid rounding issues.
  return gfx::Rect(gfx::ToCeiledSize(new_bounds.size()));
}

}  // namespace chromecast
