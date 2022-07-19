// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/highlight_border_overlay_chromeos.h"

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/tablet_state.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/views/highlight_border.h"
#include "ui/views/widget/widget.h"

namespace {

// `ImageSource` generate an image with a `view::HighlightBorder`
// painted on it.
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

}  // namespace

HighlightBorderOverlay::HighlightBorderOverlay(
    views::Widget* widget,
    const gfx::RoundedCornersF& rounded_corner)
    : layer_(ui::LAYER_NINE_PATCH),
      widget_(widget),
      rounded_corner_(rounded_corner) {
  layer_.SetFillsBoundsOpaquely(false);

  UpdateNinePatchLayer();
  UpdateLayerBounds();

  widget_->AddObserver(this);
  auto* widget_layer = widget_->GetLayer();
  widget_layer->Add(&layer_);
  widget_layer->StackAtTop(&layer_);
}

HighlightBorderOverlay::~HighlightBorderOverlay() {
  widget_->RemoveObserver(this);
}

void HighlightBorderOverlay::PaintBorder(gfx::Canvas* canvas) {
  views::HighlightBorder::PaintBorderToCanvas(
      canvas, *(widget_->GetContentsView()),
      gfx::Rect(CalculateImageSourceSize()), rounded_corner_,
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

void HighlightBorderOverlay::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  UpdateLayerVisibility();
  UpdateLayerBounds();
}

void HighlightBorderOverlay::OnWidgetThemeChanged(views::Widget* widget) {
  UpdateNinePatchLayer();
}

void HighlightBorderOverlay::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (auto* host = widget_->GetNativeWindow()->GetHost()) {
    if (host->GetDisplayId() != display.id())
      return;
  } else {
    return;
  }

  if (!(metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA))
    return;

  UpdateLayerVisibility();
}

gfx::Insets HighlightBorderOverlay::CalculateBorderRegion() const {
  // The border region should include border thickness and corner radius.
  return gfx::Insets(
      2 * views::kHighlightBorderThickness +
      std::max({rounded_corner_.upper_left(), rounded_corner_.upper_right(),
                rounded_corner_.lower_left(), rounded_corner_.lower_right()}));
}

void HighlightBorderOverlay::UpdateLayerVisibility() {
  // Hide the border when widget fills the work area.
  layer_.SetVisible(!widget_->GetWindowBoundsInScreen().Contains(
      widget_->GetWorkAreaBoundsInScreen()));
}

void HighlightBorderOverlay::UpdateLayerBounds() {
  gfx::Rect layer_bounds(widget_->GetWindowBoundsInScreen().size());
  // Outset the bounds by one border thickness for outer border.
  layer_bounds.Inset(-gfx::Insets(views::kHighlightBorderThickness));

  if (layer_bounds != layer_.bounds())
    layer_.SetBounds(layer_bounds);
}

void HighlightBorderOverlay::UpdateNinePatchLayer() {
  // Configure the nine patch layer.
  auto* border_image_source = new ImageSource(this);
  gfx::Size image_source_size = border_image_source->size();
  layer_.UpdateNinePatchLayerImage(
      gfx::ImageSkia(base::WrapUnique(border_image_source), image_source_size));

  gfx::Rect apeture(image_source_size);
  gfx::Insets border_region = CalculateBorderRegion();
  apeture.Inset(border_region);
  layer_.UpdateNinePatchLayerAperture(apeture);
  layer_.UpdateNinePatchLayerBorder(
      gfx::Rect(border_region.left(), border_region.top(),
                border_region.width(), border_region.height()));
}
