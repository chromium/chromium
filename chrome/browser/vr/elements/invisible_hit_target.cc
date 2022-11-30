// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/invisible_hit_target.h"

namespace vr {

InvisibleHitTarget::InvisibleHitTarget() {
  set_hit_testable(true);
}
InvisibleHitTarget::~InvisibleHitTarget() = default;

void InvisibleHitTarget::Render(UiElementRenderer* renderer,
                                const CameraModel& model) const {}

void InvisibleHitTarget::OnHoverEnter(const gfx::PointF& position,
                                      base::TimeTicks timestamp) {
  UiElement::OnHoverEnter(position, timestamp);
  hovered_ = true;
}

void InvisibleHitTarget::OnHoverLeave(base::TimeTicks timestamp) {
  UiElement::OnHoverLeave(timestamp);
  hovered_ = false;
}

}  // namespace vr
