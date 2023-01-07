// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/full_screen_rect.h"

#include "chrome/browser/vr/ui_element_renderer.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

FullScreenRect::FullScreenRect() = default;
FullScreenRect::~FullScreenRect() = default;

void FullScreenRect::Render(UiElementRenderer* renderer,
                            const CameraModel& model) const {
  gfx::Transform m;
  m.Scale3d(2.0f, 2.0f, 1.0f);
  renderer->DrawRadialGradientQuad(m, edge_color(), center_color(),
                                   GetClipRect(), computed_opacity(),
                                   gfx::SizeF(1.f, 1.f), corner_radii());
}

bool FullScreenRect::IsWorldPositioned() const {
  return false;
}

}  // namespace vr
