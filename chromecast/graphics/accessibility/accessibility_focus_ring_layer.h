// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_LAYER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_LAYER_H_

#include "chromecast/graphics/accessibility/accessibility_focus_ring.h"
#include "chromecast/graphics/accessibility/focus_ring_layer.h"

namespace chromecast {

// A subclass of FocusRingLayer intended for use by ChromeVox; it supports
// nonrectangular focus rings in order to highlight groups of elements or
// a range of text on a page.
class AccessibilityFocusRingLayer : public FocusRingLayer {
 public:
  AccessibilityFocusRingLayer(aura::Window* root_window,
                              AccessibilityLayerDelegate* delegate);

  AccessibilityFocusRingLayer(const AccessibilityFocusRingLayer&) = delete;
  AccessibilityFocusRingLayer& operator=(const AccessibilityFocusRingLayer&) =
      delete;

  ~AccessibilityFocusRingLayer() override;

  // Create the layer and update its bounds and position in the hierarchy.
  void Set(const AccessibilityFocusRing& ring);

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  // The outline of the current focus ring.
  AccessibilityFocusRing ring_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_LAYER_H_
