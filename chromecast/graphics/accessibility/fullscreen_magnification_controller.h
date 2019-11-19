// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_FULLSCREEN_MAGNIFICATION_CONTROLLER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_FULLSCREEN_MAGNIFICATION_CONTROLLER_H_

#include <map>
#include <memory>

#include "chromecast/graphics/accessibility/magnification_controller.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/transform.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class GestureProviderAura;
class Layer;
}  // namespace ui

namespace chromecast {

class CastGestureHandler;

class FullscreenMagnificationController : public MagnificationController,
                                          public ui::EventRewriter,
                                          public ui::GestureConsumer,
                                          public ui::LayerDelegate {
 public:
  explicit FullscreenMagnificationController(
      aura::Window* root_window,
      CastGestureHandler* cast_gesture_handler);
  ~FullscreenMagnificationController() override;

  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  void SetMagnificationScale(float magnification_scale) override;

 private:
  class GestureProviderClient;
  enum LockedGestureType { NO_GESTURE, ZOOM, SCROLL };

  // The current transform to apply to the root window to perform magnification.
  gfx::Transform GetMagnifierTransform() const;

  // Process pending gestures in |gesture_provider_|. This method returns true
  // if the controller needs to cancel existing touches.
  bool ProcessGestures();

  // Redraws the magnification window with the given origin position in dip and
  // the given scale. Returns true if the window is changed; otherwise, false.
  bool RedrawDIP(const gfx::PointF& position_in_dip, float scale);

  // Overridden from ui::EventRewriter
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // Adds the layer for the highlight-ring which provides a visual indicator
  // that magnification is enabled.
  void UpdateHighlightLayerTransform(const gfx::Transform& magnifier_transform);
  void AddHighlightLayer();

  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  aura::Window* root_window_;

  bool is_enabled_ = false;
  gfx::Transform original_transform_;

  // Stores the last touched location. This value is used on zooming to keep
  // this location visible.
  gfx::Point point_of_interest_in_root_;

  // Current scale, origin (left-top) position of the magnification window.
  float magnification_scale_;
  gfx::PointF magnification_origin_;

  float original_magnification_scale_;
  gfx::PointF original_magnification_origin_;

  // We own our own GestureProvider to detect gestures with screen coordinates
  // of touch events. As MagnificationController changes zoom level and moves
  // viewport, logical coordinates of touches cannot be used for gesture
  // detection as they are changed if the controller reacts to gestures.
  std::unique_ptr<ui::GestureProviderAura> gesture_provider_;
  std::unique_ptr<GestureProviderClient> gesture_provider_client_;

  // We lock the gesture once user performs either scroll or pinch gesture above
  // those thresholds.
  LockedGestureType locked_gesture_ = NO_GESTURE;

  // If true, consumes all touch events.
  bool consume_touch_event_ = false;

  // Number of touch points on the screen.
  int32_t touch_points_ = 0;

  // Map for holding ET_TOUCH_PRESS events. Those events are used to dispatch
  // ET_TOUCH_CANCELLED events. Events will be removed from this map when press
  // events are cancelled, i.e. size of this map can be different from number of
  // touches on the screen. Key is pointer id.
  std::map<int32_t, std::unique_ptr<ui::TouchEvent>> press_event_map_;

  CastGestureHandler* cast_gesture_handler_;

  std::unique_ptr<ui::Layer> highlight_ring_layer_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_FULLSCREEN_MAGNIFICATION_CONTROLLER_H_
