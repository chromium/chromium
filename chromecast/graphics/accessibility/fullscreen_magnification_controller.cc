// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/accessibility/fullscreen_magnification_controller.h"

#include <vector>

#include "base/numerics/ranges.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace chromecast {

namespace {
// Default ratio of magnifier scale.
constexpr float kDefaultMagnificationScale = 2.f;

constexpr float kMaxMagnifiedScale = 20.0f;
constexpr float kMinMagnifiedScaleThreshold = 1.1f;
constexpr float kNonMagnifiedScale = 1.0f;

constexpr float kZoomGestureLockThreshold = 0.1f;
constexpr float kScrollGestureLockThreshold = 20000.0f;

// The color of the highlight ring.
constexpr SkColor kHighlightRingColor = SkColorSetRGB(247, 152, 58);
constexpr int kHighlightShadowRadius = 10;
constexpr int kHighlightShadowAlpha = 90;

// Convert point locations to DIP by using the original transform, rather than
// the one currently installed on the window tree host (which might be our
// magnifier).
gfx::Point ConvertPixelsToDIPWithOriginalTransform(
    const gfx::Transform& transform,
    const gfx::Point& point) {
  gfx::Transform invert;
  if (!transform.GetInverse(&invert)) {
    // Some transforms can't be inverted, so we do the same as window tree
    // host's DIP conversion and just use the transform as is in that case.
    invert = transform;
  }
  gfx::PointF dip_point(point);
  invert.TransformPoint(&dip_point);
  return gfx::ToFlooredPoint(dip_point);
}

// Correct the given scale value if necessary.
void ValidateScale(float* scale) {
  *scale = base::ClampToRange(*scale, kNonMagnifiedScale, kMaxMagnifiedScale);
  DCHECK(kNonMagnifiedScale <= *scale && *scale <= kMaxMagnifiedScale);
}

}  // namespace

class FullscreenMagnificationController::GestureProviderClient
    : public ui::GestureProviderAuraClient {
 public:
  GestureProviderClient() = default;
  ~GestureProviderClient() override = default;

  // ui::GestureProviderAuraClient overrides:
  void OnGestureEvent(GestureConsumer* consumer,
                      ui::GestureEvent* event) override {
    // Do nothing. OnGestureEvent is for timer based gesture events, e.g. tap.
    // MagnificationController is interested only in pinch and scroll
    // gestures.
    DCHECK_NE(ui::ET_GESTURE_SCROLL_BEGIN, event->type());
    DCHECK_NE(ui::ET_GESTURE_SCROLL_END, event->type());
    DCHECK_NE(ui::ET_GESTURE_SCROLL_UPDATE, event->type());
    DCHECK_NE(ui::ET_GESTURE_PINCH_BEGIN, event->type());
    DCHECK_NE(ui::ET_GESTURE_PINCH_END, event->type());
    DCHECK_NE(ui::ET_GESTURE_PINCH_UPDATE, event->type());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureProviderClient);
};

FullscreenMagnificationController::FullscreenMagnificationController(
    aura::Window* root_window,
    CastGestureHandler* cast_gesture_handler)
    : root_window_(root_window),
      magnification_scale_(kDefaultMagnificationScale),
      cast_gesture_handler_(cast_gesture_handler) {
  DCHECK(root_window);
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);

  gesture_provider_client_ = std::make_unique<GestureProviderClient>();
  gesture_provider_ = std::make_unique<ui::GestureProviderAura>(
      this, gesture_provider_client_.get());
}

FullscreenMagnificationController::~FullscreenMagnificationController() {}

void FullscreenMagnificationController::SetEnabled(bool enabled) {
  if (is_enabled_ == enabled)
    return;
  if (!is_enabled_) {
    // Stash the original root window transform so we can restore it after we're
    // done.
    original_transform_ = root_window_->transform();
  }
  is_enabled_ = enabled;
  auto magnifier_transform(GetMagnifierTransform());
  root_window_->SetTransform(magnifier_transform);
  if (enabled) {
    // Add the highlight ring.
    if (!highlight_ring_layer_) {
      AddHighlightLayer();
    }
    UpdateHighlightLayerTransform(magnifier_transform);
  } else {
    // Remove the highlight ring.
    if (highlight_ring_layer_) {
      root_window_->layer()->Remove(highlight_ring_layer_.get());
      highlight_ring_layer_.reset();
    }
  }
}

bool FullscreenMagnificationController::IsEnabled() const {
  return is_enabled_;
}

void FullscreenMagnificationController::SetMagnificationScale(
    float magnification_scale) {
  magnification_scale_ = magnification_scale;
  root_window_->SetTransform(GetMagnifierTransform());
}

gfx::Transform FullscreenMagnificationController::GetMagnifierTransform()
    const {
  gfx::Transform transform = original_transform_;
  if (IsEnabled()) {
    transform.Scale(magnification_scale_, magnification_scale_);

    // Top corner of window.
    gfx::Point offset = gfx::ToFlooredPoint(magnification_origin_);
    transform.Translate(-offset.x(), -offset.y());
  }

  return transform;
}

// Overridden from ui::EventRewriter
ui::EventDispatchDetails FullscreenMagnificationController::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!IsEnabled()) {
    return SendEvent(continuation, &event);
  }
  if (!event.IsTouchEvent())
    return SendEvent(continuation, &event);

  const ui::TouchEvent* touch_event = event.AsTouchEvent();

  // Touch events come through in screen pixels, but untransformed. This is the
  // raw coordinate not yet mapped to the root window's coordinate system or the
  // screen. Convert it into the root window's coordinate system, in DIP which
  // is what the rest of this class expects.
  gfx::Point location = ConvertPixelsToDIPWithOriginalTransform(
      original_transform_, touch_event->location());
  gfx::Point root_location = ConvertPixelsToDIPWithOriginalTransform(
      original_transform_, touch_event->root_location());

  // We now need a TouchEvent that has its coordinates mapped into root window
  // DIP.
  ui::TouchEvent touch_event_dip = *touch_event;
  touch_event_dip.set_location(location);
  touch_event_dip.set_root_location(root_location);

  // Track finger presses so we can look for our two-finger drag.
  if (touch_event_dip.type() == ui::ET_TOUCH_PRESSED) {
    touch_points_++;
    press_event_map_[touch_event_dip.pointer_details().id] =
        std::make_unique<ui::TouchEvent>(*touch_event);
  } else if (touch_event_dip.type() == ui::ET_TOUCH_RELEASED) {
    touch_points_--;
    press_event_map_.erase(touch_event_dip.pointer_details().id);
  }

  if (gesture_provider_->OnTouchEvent(&touch_event_dip)) {
    gesture_provider_->OnTouchEventAck(
        touch_event_dip.unique_event_id(), false /* event_consumed */,
        false /* is_source_touch_event_set_non_blocking */);
  } else {
    return DiscardEvent(continuation);
  }

  // The user can change the zoom level with two fingers pinch and pan around
  // with two fingers scroll. Once we detect one of those two gestures, we start
  // consuming all touch events by cancelling existing touches. If
  // cancel_pressed_touches is set to true, ET_TOUCH_CANCELLED
  // events are dispatched for existing touches after the next for-loop.
  bool cancel_pressed_touches = ProcessGestures();

  if (cancel_pressed_touches) {
    // Start consuming all touch events after we cancel existing touches.
    consume_touch_event_ = true;

    if (!press_event_map_.empty()) {
      DCHECK_EQ(2u, press_event_map_.size());
      ui::EventDispatchDetails details;
      for (const auto& it : press_event_map_) {
        ui::TouchEvent rewritten_touch_event(
            ui::ET_TOUCH_CANCELLED, it.second->location_f(),
            it.second->root_location_f(), touch_event_dip.time_stamp(),
            it.second->pointer_details(), it.second->flags());
        details = SendEvent(continuation, &rewritten_touch_event);
        if (details.dispatcher_destroyed)
          break;
      }
      press_event_map_.clear();
      return details;
    }
  }

  bool discard = consume_touch_event_;

  // Reset state once no point is touched on the screen.
  if (touch_points_ == 0) {
    consume_touch_event_ = false;
    locked_gesture_ = NO_GESTURE;

    // Jump back to exactly 1.0 if we are just a tiny bit zoomed in.
    if (magnification_scale_ < kMinMagnifiedScaleThreshold)
      SetMagnificationScale(kNonMagnifiedScale);
  }

  if (discard)
    return DiscardEvent(continuation);

  return SendEvent(continuation, &event);
}

bool FullscreenMagnificationController::RedrawDIP(
    const gfx::PointF& position_in_dip,
    float scale) {
  DCHECK(root_window_);

  float x = position_in_dip.x();
  float y = position_in_dip.y();

  ValidateScale(&scale);

  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;

  const gfx::Size host_size_in_dip = root_window_->bounds().size();
  const float scaled_width = host_size_in_dip.width() / scale;
  const float scaled_height = host_size_in_dip.height() / scale;
  float max_x = host_size_in_dip.width() - scaled_width;
  float max_y = host_size_in_dip.height() - scaled_height;
  if (x > max_x)
    x = max_x;
  if (y > max_y)
    y = max_y;

  // Does nothing if both the origin and the scale are not changed.
  if (magnification_origin_.x() == x && magnification_origin_.y() == y &&
      scale == magnification_scale_) {
    return false;
  }

  magnification_origin_.set_x(x);
  magnification_origin_.set_y(y);
  magnification_scale_ = scale;

  auto magnifier_transform = GetMagnifierTransform();
  root_window_->SetTransform(magnifier_transform);

  UpdateHighlightLayerTransform(magnifier_transform);

  return true;
}

bool FullscreenMagnificationController::ProcessGestures() {
  bool cancel_pressed_touches = false;

  std::vector<std::unique_ptr<ui::GestureEvent>> gestures =
      gesture_provider_->GetAndResetPendingGestures();
  for (const auto& gesture : gestures) {
    const ui::GestureEventDetails& details = gesture->details();

    if (gesture->type() == ui::ET_GESTURE_END ||
        gesture->type() == ui::ET_GESTURE_BEGIN) {
      locked_gesture_ = NO_GESTURE;
      cancel_pressed_touches = false;
    }

    if (details.touch_points() != 2)
      continue;

    if (gesture->type() == ui::ET_GESTURE_PINCH_BEGIN) {
      original_magnification_scale_ = magnification_scale_;

      // Start consuming touch events with cancelling existing touches.
      if (!consume_touch_event_)
        cancel_pressed_touches = true;
    } else if (gesture->type() == ui::ET_GESTURE_PINCH_UPDATE &&
               (locked_gesture_ == NO_GESTURE || locked_gesture_ == ZOOM)) {
      float scale = magnification_scale_ * details.scale();
      ValidateScale(&scale);

      // Lock to zoom mode if the difference between our new scale and old scale
      // passes our zoom lock threshold.
      if (locked_gesture_ == NO_GESTURE &&
          std::abs(scale - original_magnification_scale_) >
              kZoomGestureLockThreshold) {
        locked_gesture_ = FullscreenMagnificationController::ZOOM;
      }

      // |details.bounding_box().CenterPoint()| return center of touch points
      // of gesture in non-dip screen coordinate.
      gfx::PointF gesture_center =
          gfx::PointF(details.bounding_box().CenterPoint());

      // Root transform does dip scaling, screen magnification scaling and
      // translation. Apply inverse transform to convert non-dip screen
      // coordinate to dip logical coordinate.
      root_window_->GetHost()->GetInverseRootTransform().TransformPoint(
          &gesture_center);

      // Calculate new origin to keep the distance between |gesture_center|
      // and |origin| same in screen coordinate. This means the following
      // equation.
      // (gesture_center.x - magnification_origin_.x) * magnification_scale_ =
      //   (gesture_center.x - new_origin.x) * scale
      // If you solve it for |new_origin|, you will get the following formula.
      const gfx::PointF origin =
          gfx::PointF(gesture_center.x() -
                          (magnification_scale_ / scale) *
                              (gesture_center.x() - magnification_origin_.x()),
                      gesture_center.y() -
                          (magnification_scale_ / scale) *
                              (gesture_center.y() - magnification_origin_.y()));

      RedrawDIP(origin, scale);

      // Invoke the tap gesture so we don't go idle in the UI while zooming.
      cast_gesture_handler_->HandleTapGesture(
          gfx::ToFlooredPoint(gesture_center));
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      original_magnification_origin_ = magnification_origin_;

      // Start consuming all touch events with cancelling existing touches.
      if (!consume_touch_event_)
        cancel_pressed_touches = true;
    } else if (gesture->type() == ui::ET_GESTURE_SCROLL_UPDATE &&
               (locked_gesture_ == NO_GESTURE || locked_gesture_ == SCROLL)) {
      // If we're not zoomed, scroll is a no-op.
      if (magnification_scale_ <= kNonMagnifiedScale) {
        continue;
      }
      float new_x = magnification_origin_.x() +
                    (-1.0f * details.scroll_x() / magnification_scale_);
      float new_y = magnification_origin_.y() +
                    (-1.0f * details.scroll_y() / magnification_scale_);

      // Lock to scroll mode if the squared distance from the old position
      // passes our scroll lock threshold.
      if (locked_gesture_ == NO_GESTURE) {
        float diff_x =
            (new_x - original_magnification_origin_.x()) * magnification_scale_;
        float diff_y =
            (new_y - original_magnification_origin_.y()) * magnification_scale_;
        float squared_distance = (diff_x * diff_x) + (diff_y * diff_y);
        if (squared_distance > kScrollGestureLockThreshold) {
          locked_gesture_ = SCROLL;
        }
      }
      RedrawDIP(gfx::PointF(new_x, new_y), magnification_scale_);

      // Invoke the tap gesture so we don't go idle in the UI whiole dragging.
      cast_gesture_handler_->HandleTapGesture(gfx::Point(new_x, new_y));
    }
  }

  return cancel_pressed_touches;
}

void FullscreenMagnificationController::AddHighlightLayer() {
  ui::Layer* root_layer = root_window_->layer();
  highlight_ring_layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  highlight_ring_layer_->set_name("MagnificationHighlightLayer");
  root_layer->Add(highlight_ring_layer_.get());
  highlight_ring_layer_->parent()->StackAtTop(highlight_ring_layer_.get());
  gfx::Rect bounds(root_layer->bounds());
  highlight_ring_layer_->SetBounds(bounds);
  highlight_ring_layer_->set_delegate(this);
  highlight_ring_layer_->SetFillsBoundsOpaquely(false);
}

void FullscreenMagnificationController::UpdateHighlightLayerTransform(
    const gfx::Transform& magnifier_transform) {
  // The highlight ring layer needs to be drawn unmagnified, so take the inverse
  // of the magnification transform.
  gfx::Transform inverse_transform;
  if (!magnifier_transform.GetInverse(&inverse_transform)) {
    LOG(ERROR) << "Unable to apply inverse transform to magnifier ring";
    return;
  }
  gfx::Transform highlight_layer_transform(original_transform_);
  highlight_layer_transform.ConcatTransform(inverse_transform);
  highlight_ring_layer_->SetTransform(highlight_layer_transform);

  // Make sure the highlight ring layer is on top.
  highlight_ring_layer_->parent()->StackAtTop(highlight_ring_layer_.get());

  // Repaint.
  highlight_ring_layer_->SchedulePaint(root_window_->layer()->bounds());
}

void FullscreenMagnificationController::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, highlight_ring_layer_->size());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2);

  flags.setColor(kHighlightRingColor);

  gfx::Rect bounds(highlight_ring_layer_->bounds());
  for (int i = 0; i < 10; i++) {
    // Fade out alpha quadratically.
    flags.setAlpha(
        (kHighlightShadowAlpha * std::pow(kHighlightShadowRadius - i, 2)) /
        std::pow(kHighlightShadowRadius, 2));

    gfx::Rect outsetRect = bounds;
    outsetRect.Inset(i, i, i, i);
    recorder.canvas()->DrawRect(outsetRect, flags);
  }
}

void FullscreenMagnificationController::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

}  // namespace chromecast
