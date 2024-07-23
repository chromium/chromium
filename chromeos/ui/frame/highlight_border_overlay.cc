// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/highlight_border_overlay.h"

#include <algorithm>
#include <map>

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/highlight_border.h"
#include "ui/views/widget/widget.h"

namespace {

// Currently, each dark and light mode has only one set of highlight and border
// colors. The windows that are using HighlightBorderOverlay have three
// different rounded corner radius. There should be 6 different types of image
// sources for highlight border.
constexpr size_t kMaxImageSourceNum = 6;

constexpr views::HighlightBorder::Type kBorderType =
    views::HighlightBorder::Type::kHighlightBorderOnShadow;

// A highlight border overlay is featured by its highlight color, border color,
// and rounded corner radius.
struct HighlightBorderProperties {
  bool operator<(const HighlightBorderProperties& other) const {
    return std::tie(highlight_color, border_color, corner_radius) <
           std::tie(other.highlight_color, other.border_color,
                    other.corner_radius);
  }

  SkColor highlight_color = SK_ColorTRANSPARENT;
  SkColor border_color = SK_ColorTRANSPARENT;
  int corner_radius = 0;
};

// Generates an image painted with a highlight border.
class HighlightBorderImageSource : public gfx::CanvasImageSource {
 public:
  HighlightBorderImageSource(const gfx::Size& size,
                             const HighlightBorderProperties& properties)
      : gfx::CanvasImageSource(size), properties_(properties) {}
  HighlightBorderImageSource(const HighlightBorderImageSource&) = delete;
  HighlightBorderImageSource& operator=(const HighlightBorderImageSource&) =
      delete;
  ~HighlightBorderImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    views::HighlightBorder::PaintBorderToCanvas(
        canvas, properties_.highlight_color, properties_.border_color,
        gfx::Rect(size()), gfx::RoundedCornersF(properties_.corner_radius),
        kBorderType);
  }

 private:
  const HighlightBorderProperties properties_;
};

gfx::ImageSkia GetHighlightBorderImageMatching(
    const gfx::Size& size,
    const HighlightBorderProperties& properties) {
  static base::NoDestructor<std::map<HighlightBorderProperties, gfx::ImageSkia>>
      image_source_cache;

  auto iter = image_source_cache->find(properties);
  if (iter != image_source_cache->end()) {
    return iter->second;
  }

  // Evict the images that have no owners.
  std::erase_if(*image_source_cache, [](auto& key_and_image_source) {
    return key_and_image_source.second.IsUniquelyOwned();
  });

  // Create and store a new image.
  const auto& [iterator, inserted] = image_source_cache->emplace(
      properties, gfx::ImageSkia(std::make_unique<HighlightBorderImageSource>(
                                     size, properties),
                                 /*scale=*/1.0f));
  DCHECK(inserted);
  DCHECK_LE(image_source_cache->size(), kMaxImageSourceNum);

  return iterator->second;
}

}  // namespace

HighlightBorderOverlay::HighlightBorderOverlay(views::Widget* widget)
    : layer_(ui::LAYER_NINE_PATCH),
      widget_(widget),
      window_(widget->GetNativeWindow()) {
  rounded_corner_radius_ =
      std::max(0, window_->GetProperty(aura::client::kWindowCornerRadiusKey));
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

gfx::Size HighlightBorderOverlay::CalculateImageSourceSize() const {
  // Initialize the image source bounds with 1 dp of center patch size.
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

  // We need to update the highlight border radius to match the radius of the
  // frame.
  if (key == aura::client::kWindowCornerRadiusKey) {
    const int corner_radius =
        std::max(0, window_->GetProperty(aura::client::kWindowCornerRadiusKey));
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

  // TabletState might be nullptr in some tests.
  const bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  const auto window_state_type =
      window_->GetProperty(chromeos::kWindowStateTypeKey);

  const bool show_border_in_tablet_mode =
      in_tablet_mode &&
      window_state_type == chromeos::WindowStateType::kFloated &&
      window_state_type == chromeos::WindowStateType::kPip;
  const bool show_border_in_clamshell_mode =
      !in_tablet_mode &&
      window_state_type != chromeos::WindowStateType::kFullscreen;

  const gfx::Insets border_region = CalculateBorderRegion();
  const bool fits_layer_bounds =
      border_region.width() <= layer_bounds.width() &&
      border_region.height() <= layer_bounds.height();

  // Hide the border if it's in tablet mode and the window is not float nor
  // pip or if it's in clamshell mode and the window is in fullscreen state.
  // Also hide the border if the border region is wider or higher than the
  // window since border is in layer space. It cannot exceed the bounds of the
  // layer.
  const bool show_border =
      fits_layer_bounds &&
      (show_border_in_clamshell_mode || show_border_in_tablet_mode);

  if (!show_border) {
    layer_.SetVisible(false);
    return;
  }

  layer_.SetVisible(true);
  if (layer_bounds != layer_.bounds())
    layer_.SetBounds(layer_bounds);
}

void HighlightBorderOverlay::UpdateNinePatchLayer() {
  const gfx::Size image_source_size = CalculateImageSourceSize();

  // Get the highlight border features.
  const views::View& view = *(widget_->GetContentsView());
  SkColor highlight_color =
      views::HighlightBorder::GetHighlightColor(view, kBorderType);
  SkColor border_color =
      views::HighlightBorder::GetBorderColor(view, kBorderType);
  const HighlightBorderProperties properties(highlight_color, border_color,
                                             rounded_corner_radius_);

  layer_.UpdateNinePatchLayerImage(
      GetHighlightBorderImageMatching(image_source_size, properties));

  gfx::Rect aperture(image_source_size);
  gfx::Insets border_region = CalculateBorderRegion();
  aperture.Inset(border_region);
  layer_.UpdateNinePatchLayerAperture(aperture);
  layer_.UpdateNinePatchLayerBorder(
      gfx::Rect(border_region.left(), border_region.top(),
                border_region.width(), border_region.height()));
}
