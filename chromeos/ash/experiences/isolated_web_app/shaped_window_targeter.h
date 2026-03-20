// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_SHAPED_WINDOW_TARGETER_H_
#define CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_SHAPED_WINDOW_TARGETER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// A `WindowTargeter` for a window that uses a custom shape for hit-testing. The
// window shape is given by the union of `shape_rects`. Events outside the
// rectangles fall through to the next targeter in the chain.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ISOLATED_WEB_APP)
    ShapedWindowTargeter : public aura::WindowTargeter {
 public:
  // The union of all `shape_rects` specifies the shape of the window.
  // Rectangles are in window local coordinates.
  explicit ShapedWindowTargeter(std::vector<gfx::Rect> shape_rects);

  ShapedWindowTargeter(const ShapedWindowTargeter&) = delete;
  ShapedWindowTargeter& operator=(const ShapedWindowTargeter&) = delete;

  ~ShapedWindowTargeter() override;

 private:
  // aura::WindowTargeter:
  std::unique_ptr<aura::WindowTargeter::HitTestRects> GetExtraHitTestShapeRects(
      aura::Window* target) const override;

  // The rectangles whose union define the shape of the window. Coordinates are
  // in the window's local coordinate space.
  std::vector<gfx::Rect> shape_rects_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_SHAPED_WINDOW_TARGETER_H_
