// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/accessibility/partial_magnification_controller.h"

#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromecast {
namespace {

// Default ratio of magnifier scale.
const float kDefaultMagnificationScale = 2.f;

// Radius of the magnifying glass in DIP. This does not include the thickness
// of the magnifying glass shadow and border.
const int kMagnifierRadius = 188;
// Size of the border around the magnifying glass in DIP.
const int kBorderSize = 10;
// Thickness of the outline around magnifying glass border in DIP.
const int kBorderOutlineThickness = 1;
// Thickness of the shadow around the magnifying glass in DIP.
const int kShadowThickness = 24;
// Offset of the shadow around the magnifying glass in DIP. One of the shadows
// is lowered a bit, so we have to include |kShadowOffset| in our calculations
// to compensate.
const int kShadowOffset = 24;
// The color of the border and its outlines. The border has an outline on both
// sides, producing a black/white/black ring.
const SkColor kBorderColor = SkColorSetARGB(204, 255, 255, 255);
const SkColor kBorderOutlineColor = SkColorSetARGB(51, 0, 0, 0);
// The colors of the two shadow around the magnifiying glass.
const SkColor kTopShadowColor = SkColorSetARGB(26, 0, 0, 0);
const SkColor kBottomShadowColor = SkColorSetARGB(61, 0, 0, 0);
// Inset on the zoom filter.
const int kZoomInset = 0;
// Vertical offset between the center of the magnifier and the tip of the
// pointer.
const int kVerticalOffset = 0;

// Name of the magnifier window.
const char kPartialMagniferWindowName[] = "PartialMagnifierWindow";

gfx::Size GetWindowSize() {
  // The diameter of the window is the diameter of the magnifier, border and
  // shadow combined. We apply |kShadowOffset| on all sides even though the
  // shadow is only thicker on the bottom so as to keep the circle centered in
  // the view and keep calculations (border rendering and content masking)
  // simpler.
  int window_diameter =
      (kMagnifierRadius + kBorderSize + kShadowThickness + kShadowOffset) * 2;
  return gfx::Size(window_diameter, window_diameter);
}

gfx::Rect GetBounds(gfx::Point mouse) {
  gfx::Size size = GetWindowSize();
  gfx::Point origin(mouse.x() - (size.width() / 2),
                    mouse.y() - (size.height() / 2) - kVerticalOffset);
  return gfx::Rect(origin, size);
}

}  // namespace

// The content mask provides a clipping layer for the magnification window so we
// can show a circular magnifier.
class PartialMagnificationController::ContentMask : public ui::LayerDelegate {
 public:
  // If |is_border| is true, the circle will be a stroke. This is useful if we
  // wish to clip a border.
  ContentMask(bool is_border, gfx::Size mask_bounds)
      : layer_(ui::LAYER_TEXTURED), is_border_(is_border) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
    layer_.SetBounds(gfx::Rect(mask_bounds));
  }

  ContentMask(const ContentMask&) = delete;
  ContentMask& operator=(const ContentMask&) = delete;

  ~ContentMask() override { layer_.set_delegate(nullptr); }

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer()->size());

    cc::PaintFlags flags;
    flags.setAlpha(255);
    flags.setAntiAlias(true);
    // Stroke is used for clipping the border which consists of the rendered
    // border |kBorderSize| and the magnifier shadow |kShadowThickness| and
    // |kShadowOffset|.
    flags.setStrokeWidth(kBorderSize + kShadowThickness + kShadowOffset);
    flags.setStyle(is_border_ ? cc::PaintFlags::kStroke_Style
                              : cc::PaintFlags::kFill_Style);

    // If we want to clip the magnifier zone use the magnifiers radius.
    // Otherwise we want to clip the border, shadow and shadow offset so we
    // start
    // at the halfway point of the stroke width.
    gfx::Rect rect(layer()->bounds().size());
    int clipping_radius = kMagnifierRadius;
    if (is_border_)
      clipping_radius += (kShadowThickness + kShadowOffset + kBorderSize) / 2;
    recorder.canvas()->DrawCircle(rect.CenterPoint(), clipping_radius, flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    // Redrawing will take care of scale factor change.
  }

  ui::Layer layer_;
  bool is_border_;
};

// The border renderer draws the border as well as outline on both the outer and
// inner radius to increase visibility. The border renderer also handles drawing
// the shadow.
class PartialMagnificationController::BorderRenderer
    : public ui::LayerDelegate {
 public:
  explicit BorderRenderer(const gfx::Rect& window_bounds)
      : magnifier_window_bounds_(window_bounds) {
    magnifier_shadows_.push_back(gfx::ShadowValue(
        gfx::Vector2d(0, kShadowOffset), kShadowThickness, kBottomShadowColor));
    magnifier_shadows_.push_back(gfx::ShadowValue(
        gfx::Vector2d(0, 0), kShadowThickness, kTopShadowColor));
  }

  BorderRenderer(const BorderRenderer&) = delete;
  BorderRenderer& operator=(const BorderRenderer&) = delete;

  ~BorderRenderer() override = default;

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, magnifier_window_bounds_.size());

    // Draw the shadow.
    cc::PaintFlags shadow_flags;
    shadow_flags.setAntiAlias(true);
    shadow_flags.setColor(SK_ColorTRANSPARENT);
    shadow_flags.setLooper(gfx::CreateShadowDrawLooper(magnifier_shadows_));
    gfx::Rect shadow_bounds(magnifier_window_bounds_.size());
    recorder.canvas()->DrawCircle(
        shadow_bounds.CenterPoint(),
        shadow_bounds.width() / 2 - kShadowThickness - kShadowOffset,
        shadow_flags);

    cc::PaintFlags border_flags;
    border_flags.setAntiAlias(true);
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);

    // The radius of the magnifier and its border.
    const int magnifier_radius = kMagnifierRadius + kBorderSize;

    // Draw the inner border.
    border_flags.setStrokeWidth(kBorderSize);
    border_flags.setColor(kBorderColor);
    recorder.canvas()->DrawCircle(magnifier_window_bounds_.CenterPoint(),
                                  magnifier_radius - kBorderSize / 2,
                                  border_flags);

    // Draw border outer outline and then draw the border inner outline.
    border_flags.setStrokeWidth(kBorderOutlineThickness);
    border_flags.setColor(kBorderOutlineColor);
    recorder.canvas()->DrawCircle(
        magnifier_window_bounds_.CenterPoint(),
        magnifier_radius - kBorderOutlineThickness / 2, border_flags);
    recorder.canvas()->DrawCircle(
        magnifier_window_bounds_.CenterPoint(),
        magnifier_radius - kBorderSize + kBorderOutlineThickness / 2,
        border_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  gfx::Rect magnifier_window_bounds_;
  std::vector<gfx::ShadowValue> magnifier_shadows_;
};

PartialMagnificationController::PartialMagnificationController(
    aura::Window* root_window)
    : magnification_scale_(kDefaultMagnificationScale),
      root_window_(root_window) {
  root_window_->AddPreTargetHandler(this);
}

PartialMagnificationController::~PartialMagnificationController() {
  CloseMagnifierWindow();
  root_window_->RemovePreTargetHandler(this);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void PartialMagnificationController::SetEnabled(bool enabled) {
  is_enabled_ = enabled;
  SetActive(false);
}

bool PartialMagnificationController::IsEnabled() const {
  return is_enabled_;
}

void PartialMagnificationController::SwitchTargetRootWindowIfNeeded(
    aura::Window* new_root_window) {
  if (host_widget_ &&
      new_root_window == host_widget_->GetNativeView()->GetRootWindow())
    return;

  if (!new_root_window)
    new_root_window = root_window_;

  if (is_enabled_ && is_active_) {
    CloseMagnifierWindow();
    CreateMagnifierWindow(new_root_window);
  }
}

void PartialMagnificationController::OnWindowDestroying(aura::Window* window) {
  CloseMagnifierWindow();

  aura::Window* new_root_window = root_window_;
  if (new_root_window != window)
    SwitchTargetRootWindowIfNeeded(new_root_window);
}

void PartialMagnificationController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, host_widget_);
  RemoveZoomWidgetObservers();
  host_widget_ = nullptr;
}

void PartialMagnificationController::SetActive(bool active) {
  // Fail if we're trying to activate while disabled.
  DCHECK(is_enabled_ || !active);

  is_active_ = active;
  if (is_active_) {
    CreateMagnifierWindow(root_window_);
  } else {
    CloseMagnifierWindow();
  }
}

void PartialMagnificationController::OnTouchEvent(ui::TouchEvent* event) {
  if (!is_enabled_) {
    return;
  }

  // Compute the event location in screen space.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &screen_point);

  // TODO(rdaum): Touch pressed is probably not what we want here, we'll
  // probably want a specific gesture for dragging the magnifier around.
  if (event->type() == ui::ET_TOUCH_PRESSED) {
    SetActive(true);
  }

  if (event->type() == ui::ET_TOUCH_RELEASED)
    SetActive(false);

  if (!is_active_)
    return;

  // If the previous root window was detached host_widget_ will be null;
  // reconstruct it. We also need to change the root window if the cursor has
  // crossed display boundries.
  SwitchTargetRootWindowIfNeeded(root_window_);

  // If that failed for any reason return.
  if (!host_widget_) {
    SetActive(false);
    return;
  }

  // Remap point from where it was captured to the display it is actually on.
  gfx::Point point = event->root_location();
  aura::Window::ConvertPointToTarget(
      event_root, host_widget_->GetNativeView()->GetRootWindow(), &point);
  host_widget_->SetBounds(GetBounds(point));
}

void PartialMagnificationController::CreateMagnifierWindow(
    aura::Window* root_window) {
  if (host_widget_ || !root_window)
    return;

  root_window->AddObserver(this);

  gfx::Point mouse(
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());

  host_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  params.bounds = GetBounds(mouse);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = root_window;
  host_widget_->Init(std::move(params));
  host_widget_->set_focus_on_creation(false);
  host_widget_->Show();

  aura::Window* window = host_widget_->GetNativeView();
  window->SetName(kPartialMagniferWindowName);

  ui::Layer* root_layer = host_widget_->GetNativeView()->layer();

  zoom_layer_.reset(new ui::Layer(ui::LayerType::LAYER_SOLID_COLOR));
  zoom_layer_->SetBounds(gfx::Rect(GetWindowSize()));
  zoom_layer_->SetBackgroundZoom(magnification_scale_, kZoomInset);
  root_layer->Add(zoom_layer_.get());

  border_layer_.reset(new ui::Layer(ui::LayerType::LAYER_TEXTURED));
  border_layer_->SetBounds(gfx::Rect(GetWindowSize()));
  border_renderer_.reset(new BorderRenderer(gfx::Rect(GetWindowSize())));
  border_layer_->set_delegate(border_renderer_.get());
  border_layer_->SetFillsBoundsOpaquely(false);
  root_layer->Add(border_layer_.get());

  border_mask_.reset(new ContentMask(true, GetWindowSize()));
  border_layer_->SetMaskLayer(border_mask_->layer());

  zoom_mask_.reset(new ContentMask(false, GetWindowSize()));
  zoom_layer_->SetMaskLayer(zoom_mask_->layer());

  host_widget_->AddObserver(this);
}

void PartialMagnificationController::CloseMagnifierWindow() {
  if (host_widget_) {
    RemoveZoomWidgetObservers();
    host_widget_->Close();
    host_widget_ = nullptr;
  }
}

void PartialMagnificationController::RemoveZoomWidgetObservers() {
  DCHECK(host_widget_);
  host_widget_->RemoveObserver(this);
  aura::Window* root_window = host_widget_->GetNativeView()->GetRootWindow();
  DCHECK(root_window);
  root_window->RemoveObserver(this);
}

void PartialMagnificationController::SetMagnificationScale(
    float magnification_scale) {
  magnification_scale_ = magnification_scale;
  // TODO(rdaum): This is probably going to require a redraw/refresh if the
  // magnifier is currently visible.
}

}  // namespace chromecast
