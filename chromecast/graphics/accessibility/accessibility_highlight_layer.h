// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_HIGHLIGHT_LAYER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_HIGHLIGHT_LAYER_H_

#include <vector>

#include "chromecast/graphics/accessibility/accessibility_layer.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace chromecast {

// A subclass of LayerDelegate that can highlight regions on the screen.
class AccessibilityHighlightLayer : public AccessibilityLayer {
 public:
  explicit AccessibilityHighlightLayer(aura::Window* root_window,
                                       AccessibilityLayerDelegate* delegate);

  AccessibilityHighlightLayer(const AccessibilityHighlightLayer&) = delete;
  AccessibilityHighlightLayer& operator=(const AccessibilityHighlightLayer&) =
      delete;

  ~AccessibilityHighlightLayer() override;

  // Create the layer and update its bounds and position in the hierarchy.
  void Set(const std::vector<gfx::Rect>& rects, SkColor color);

  // AccessibilityLayer overrides:
  bool CanAnimate() const override;
  int GetInset() const override;

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  // The current rects to be highlighted.
  std::vector<gfx::Rect> rects_;

  // The highlight color.
  SkColor highlight_color_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_HIGHLIGHT_LAYER_H_
