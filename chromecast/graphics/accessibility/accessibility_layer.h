// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_LAYER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_LAYER_H_

#include <memory>

#include "base/time/time.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class Compositor;
class Layer;
}  // namespace ui

namespace chromecast {

// A delegate interface implemented by the object that owns an
// AccessibilityLayer.
class AccessibilityLayerDelegate {
 public:
  virtual void OnDeviceScaleFactorChanged() = 0;
  virtual void OnAnimationStep(base::TimeTicks timestamp) = 0;

 protected:
  virtual ~AccessibilityLayerDelegate() {}
};

// AccessibilityLayer manages a global always-on-top layer used to
// highlight or annotate UI elements for accessibility.
class AccessibilityLayer : public ui::LayerDelegate,
                           public ui::CompositorAnimationObserver {
 public:
  AccessibilityLayer(aura::Window* root_window,
                     AccessibilityLayerDelegate* delegate);

  AccessibilityLayer(const AccessibilityLayer&) = delete;
  AccessibilityLayer& operator=(const AccessibilityLayer&) = delete;

  ~AccessibilityLayer() override;

  // Move the accessibility layer to the given bounds in the coordinates of
  // the given root window.
  void Set(aura::Window* root_window, const gfx::Rect& bounds);

  // Set the layer's opacity.
  void SetOpacity(float opacity);

  // Returns true if this layer is in a composited window with an
  // animation observer.
  virtual bool CanAnimate() const = 0;

  // Gets the inset for this layer in DIPs. This is used to increase
  // the bounding box to provide space for any margins or padding.
  virtual int GetInset() const = 0;

  ui::Layer* layer() { return layer_.get(); }
  aura::Window* root_window() { return root_window_; }

 protected:
  // Updates |root_window_| and creates |layer_| if it doesn't exist,
  // or if the root window has changed. Moves the layer to the top if
  // it wasn't there already.
  void CreateOrUpdateLayer(aura::Window* root_window,
                           const char* layer_name,
                           const gfx::Rect& bounds);

  aura::Window* root_window_ = nullptr;

  // The current layer.
  std::unique_ptr<ui::Layer> layer_;

  // The compositor associated with this layer.
  ui::Compositor* compositor_ = nullptr;

  // The bounding rectangle of the focused object, in |root_window_|
  // coordinates.
  gfx::Rect layer_rect_;

 private:
  // ui::LayerDelegate overrides:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // CompositorAnimationObserver overrides:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // The object that owns this layer.
  AccessibilityLayerDelegate* delegate_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_LAYER_H_
