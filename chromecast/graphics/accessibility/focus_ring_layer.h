// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_FOCUS_RING_LAYER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_FOCUS_RING_LAYER_H_

#include "chromecast/graphics/accessibility/accessibility_layer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace chromecast {

// FocusRingLayer draws a focus ring at a given global rectangle.
class FocusRingLayer : public AccessibilityLayer {
 public:
  explicit FocusRingLayer(aura::Window* root_window,
                          AccessibilityLayerDelegate* delegate);

  FocusRingLayer(const FocusRingLayer&) = delete;
  FocusRingLayer& operator=(const FocusRingLayer&) = delete;

  ~FocusRingLayer() override;

  // AccessibilityLayer overrides:
  bool CanAnimate() const override;
  int GetInset() const override;

  // Set a custom color, or reset to the default.
  void SetColor(SkColor color);
  void ResetColor();

 protected:
  bool has_custom_color() { return custom_color_.has_value(); }
  SkColor custom_color() { return *custom_color_; }

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  absl::optional<SkColor> custom_color_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_FOCUS_RING_LAYER_H_
