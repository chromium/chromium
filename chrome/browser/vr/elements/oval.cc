// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/oval.h"

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

Oval::Oval() = default;
Oval::~Oval() = default;

void Oval::NotifyClientSizeAnimated(const gfx::SizeF& size,
                                    int target_property_id,
                                    cc::KeyframeModel* keyframe_model) {
  Rect::NotifyClientSizeAnimated(size, target_property_id, keyframe_model);
  if (target_property_id == BOUNDS)
    SetCornerRadius(0.5f * std::min(size.height(), size.width()));
}

}  // namespace vr
