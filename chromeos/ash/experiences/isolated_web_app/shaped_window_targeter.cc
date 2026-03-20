// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/shaped_window_targeter.h"

#include <memory>
#include <vector>

#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

ShapedWindowTargeter::ShapedWindowTargeter(std::vector<gfx::Rect> shape_rects)
    : shape_rects_(std::move(shape_rects)) {}

ShapedWindowTargeter::~ShapedWindowTargeter() = default;

std::unique_ptr<aura::WindowTargeter::HitTestRects>
ShapedWindowTargeter::GetExtraHitTestShapeRects(aura::Window* target) const {
  return std::make_unique<aura::WindowTargeter::HitTestRects>(shape_rects_);
}

}  // namespace ash
