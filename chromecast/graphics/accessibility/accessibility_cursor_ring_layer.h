// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_CURSOR_RING_LAYER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_CURSOR_RING_LAYER_H_

#include "chromecast/graphics/accessibility/accessibility_focus_ring.h"
#include "chromecast/graphics/accessibility/focus_ring_layer.h"

namespace chromecast {

// A subclass of FocusRingLayer that highlights the mouse cursor while it's
// moving, to make it easier to find visually.
class AccessibilityCursorRingLayer : public FocusRingLayer {
 public:
  AccessibilityCursorRingLayer(aura::Window* root_window,
                               AccessibilityLayerDelegate* delegate,
                               int red,
                               int green,
                               int blue);

  AccessibilityCursorRingLayer(const AccessibilityCursorRingLayer&) = delete;
  AccessibilityCursorRingLayer& operator=(const AccessibilityCursorRingLayer&) =
      delete;

  ~AccessibilityCursorRingLayer() override;

  // Create the layer and update its bounds and position in the hierarchy.
  void Set(const gfx::Point& location);

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  // The current location.
  gfx::Point location_;

  int red_;
  int green_;
  int blue_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_CURSOR_RING_LAYER_H_
