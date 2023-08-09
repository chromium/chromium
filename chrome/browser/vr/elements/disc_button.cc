// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/disc_button.h"

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"

#include "ui/gfx/geometry/point_f.h"

namespace vr {

DiscButton::DiscButton(base::RepeatingCallback<void()> click_handler,
                       const gfx::VectorIcon& icon)
    : VectorIconButton(click_handler, icon) {}

DiscButton::~DiscButton() = default;

void DiscButton::OnSetCornerRadii(const CornerRadii& radii) {
  NOTREACHED();
}

void DiscButton::OnSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                gfx::KeyframeModel* keyframe_model) {
  Button::OnSizeAnimated(size, target_property_id, keyframe_model);
  if (target_property_id == BOUNDS) {
    background()->SetSize(size.width(), size.height());
    background()->SetCornerRadius(size.width() * 0.5f);  // Creates a circle.
    foreground()->SetSize(size.width() * icon_scale_factor(),
                          size.height() * icon_scale_factor());
    hit_plane()->SetSize(size.width(), size.height());
    hit_plane()->SetCornerRadius(size.width() * 0.5f);  // Creates a circle.
  }
}

}  // namespace vr
