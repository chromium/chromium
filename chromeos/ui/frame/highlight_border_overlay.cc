// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/highlight_border_overlay.h"

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/highlight_border.h"
#include "ui/views/widget/widget.h"

namespace {

// `ImageSource` generates an image painted with a highlight border.
class ImageSource : public gfx::CanvasImageSource {
 public:
  explicit ImageSource(HighlightBorderOverlay* highlight_border_layer)
      : gfx::CanvasImageSource(
            highlight_border_layer->CalculateImageSourceSize()),
        highlight_border_layer_(highlight_border_layer) {}
  ImageSource(const ImageSource&) = delete;
  ImageSource& operator=(const ImageSource&) = delete;
  ~ImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    highlight_border_layer_->PaintBorder(canvas);
  }

 private:
  base::raw_ptr<HighlightBorderOverlay> highlight_border_layer_;
};

int GetRoundedCornerRadius(chromeos::WindowStateType type) {
  if (type == chromeos::WindowStateType::kPip)
    return chromeos::kPipRoundedCornerRadius;

  return IsNormalWindowStateType(type) ? chromeos::kTopCornerRadiusWhenRestored
                                       : 0;
}

}  // namespace

HighlightBorderOverlay::HighlightBorderOverlay(views::Widget* widget)
    : layer_(ui::LAYER_NINE_PATCH),
      widget_(widget),
      window_(widget->GetNativeWindow()) {
  rounded_corner_radius_ = GetRoundedCornerRadius(
      window_->GetProperty(chromeos::kWindowStateTypeKey));
  layer_.SetFillsBoundsOpaquely(false);

  UpdateNinePatchLayer();
  UpdateLayerVisibilityAndBounds();

  window_->AddObserver(this);
  auto* widget_layer = widget_->GetLayer();
  widget_layer->Add(&layer_);
  widget_layer->StackAtTop(&layer_);
}

HighlightBorderOverlay::~HighlightBorderOverlay() {
  if (window_)
    window_->RemoveObserver(this);
}

void HighlightBorderOverlay::PaintBorder(gfx::Canvas* canvas) {
  views::HighlightBorder::PaintBorderToCanvas(
      canvas, *(widget_->GetContentsView()),
      gfx::Rect(CalculateImageSourceSize()),
      gfx::RoundedCornersF(rounded_corner_radius_),
      views::HighlightBorder::Type::kHighlightBorder3,
      /*use_light_colors=*/false);
}

gfx::Size HighlightBorderOverlay::CalculateImageSourceSize() const {
  // Initialize the image source bounds with 1 px of center patch size.
  gfx::Rect image_source_bounds(1, 1);

  // Outset the bounds with border region.
  image_source_bounds.Inset(-CalculateBorderRegion());
  return image_source_bounds.size();
}

void HighlightBorderOverlay::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  UpdateLayerVisibilityAndBounds();
}

void HighlightBorderOverlay::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == chromeos::kFrameActiveColorKey) {
    if (window->GetProperty(chromeos::kFrameActiveColorKey) !=
        static_cast<SkColor>(old)) {
      UpdateNinePatchLayer();
    }
    return;
  }

  if (key == chromeos::kWindowStateTypeKey) {
    const int corner_radius = GetRoundedCornerRadius(
        window->GetProperty(chromeos::kWindowStateTypeKey));
    if (rounded_corner_radius_ != corner_radius) {
      rounded_corner_radius_ = corner_radius;
      UpdateNinePatchLayer();
    }
    UpdateLayerVisibilityAndBounds();
  }
}

void HighlightBorderOverlay::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void HighlightBorderOverlay::OnDisplayTabletStateChanged(
    display::TabletState state) {
  UpdateLayerVisibilityAndBounds();
}

gfx::Insets HighlightBorderOverlay::CalculateBorderRegion() const {
  // The border region should include border thickness and corner radius.
  return gfx::Insets(2 * views::kHighlightBorderThickness +
                     rounded_corner_radius_);
}

void HighlightBorderOverlay::UpdateLayerVisibilityAndBounds() {
  gfx::Rect layer_bounds(widget_->GetWindowBoundsInScreen().size());
  // Outset the bounds by one border thickness for outer border.
  layer_bounds.Inset(-gfx::Insets(views::kHighlightBorderThickness));

  const gfx::Insets border_region = CalculateBorderRegion();
  // Hide the border if it's in tablet mode and the window is not float nor
  // pip or if it's in clamshell mode and the window is in fullscreen state.
  // Also hide the border if the border region is wider or higher than the
  // window since border is in layer space. It cannot exceed the bounds of the
  // layer.
  const auto window_state_type =
      window_->GetProperty(chromeos::kWindowStateTypeKey);

  // TabletState might be nullptr in some tests.
  const bool in_tablet_mode = chromeos::TabletState::Get() &&
                              chromeos::TabletState::Get()->InTabletMode();
  if ((in_tablet_mode &&
       window_state_type != chromeos::WindowStateType::kFloated &&
       window_state_type != chromeos::WindowStateType::kPip) ||
      (!in_tablet_mode &&
       window_state_type == chromeos::WindowStateType::kFullscreen) ||
      border_region.width() > layer_bounds.width() ||
      border_region.height() > layer_bounds.height()) {
    layer_.SetVisible(false);
    return;
  }

  layer_.SetVisible(true);
  if (layer_bounds != layer_.bounds())
    layer_.SetBounds(layer_bounds);
}

void HighlightBorderOverlay::UpdateNinePatchLayer() {
  // Configure the nine patch layer.
  auto* border_image_source = new ImageSource(this);
  gfx::Size image_source_size = border_image_source->size();
  layer_.UpdateNinePatchLayerImage(
      gfx::ImageSkia(base::WrapUnique(border_image_source), image_source_size));

  gfx::Rect aperture(image_source_size);
  gfx::Insets border_region = CalculateBorderRegion();
  aperture.Inset(border_region);
  layer_.UpdateNinePatchLayerAperture(aperture);
  layer_.UpdateNinePatchLayerBorder(
      gfx::Rect(border_region.left(), border_region.top(),
                border_region.width(), border_region.height()));
}
