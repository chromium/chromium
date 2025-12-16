// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_blur_layer_delegate.h"

#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "cc/paint/render_surface_filters.h"
#include "components/lens/lens_features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

namespace lens {

LensOverlayBlurLayerDelegate::LensOverlayBlurLayerDelegate(
    content::RenderWidgetHost* background_view_host)
    : background_view_host_(background_view_host) {
  SetLayer(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
  layer()->SetFillsBoundsOpaquely(true);
  layer()->set_delegate(this);

  render_widget_host_observer_.Observe(background_view_host);
}

LensOverlayBlurLayerDelegate::~LensOverlayBlurLayerDelegate() = default;

void LensOverlayBlurLayerDelegate::StartBackgroundImageCapture() {
  // If there is no background_view_host_, there is nothing to take a screenshot
  // of, so we should exit early.
  if (IsLiveBlurActive() || !background_view_host_) {
    return;
  }
  // Start taking screenshots to render on the layer.
  screenshot_timer_.Start(
      FROM_HERE,
      base::Hertz(lens::features::GetLensOverlayCustomBlurRefreshRateHertz()),
      base::BindRepeating(&LensOverlayBlurLayerDelegate::FetchBackgroundImage,
                          weak_factory_.GetWeakPtr()));
}

void LensOverlayBlurLayerDelegate::StopBackgroundImageCapture() {
  if (!IsLiveBlurActive()) {
    return;
  }
  screenshot_timer_.Stop();
}

void LensOverlayBlurLayerDelegate::Hide() {
  render_widget_host_observer_.Reset();
  background_view_host_ = nullptr;

  StopBackgroundImageCapture();
  background_screenshot_.reset();
  layer()->SchedulePaint(gfx::Rect(layer()->size()));
}

void LensOverlayBlurLayerDelegate::Show(
    content::RenderWidgetHost* background_view_host) {
  background_view_host_ = background_view_host;
  render_widget_host_observer_.Observe(background_view_host);
  StartBackgroundImageCapture();
}

bool LensOverlayBlurLayerDelegate::IsLiveBlurActive() {
  return screenshot_timer_.IsRunning();
}

bool LensOverlayBlurLayerDelegate::IsCapturingBackgroundImageForTesting() {
  return screenshot_timer_.IsRunning();
}

void LensOverlayBlurLayerDelegate::OnPaintLayer(
    const ui::PaintContext& context) {
  if (background_screenshot_.drawsNothing()) {
    return;
  }

  // Create the blur filter to apply to the downsampled image.
  cc::FilterOperations operations;
  operations.Append(cc::FilterOperation::CreateBlurFilter(
      lens::features::GetLensOverlayCustomBlurBlurRadiusPixels(),
      SkTileMode::kClamp));
  sk_sp<cc::PaintFilter> filter =
      cc::RenderSurfaceFilters::BuildImageFilter(operations);

  // Create a paint object with the filter
  cc::PaintFlags filter_flags;
  filter_flags.setImageFilter(filter);

  // Configure `canvas`.
  gfx::SizeF layer_size(layer()->size());
  ui::PaintRecorder recorder(context, gfx::ToFlooredSize(layer_size));
  gfx::Canvas* const canvas = recorder.canvas();

  gfx::ImageSkia image =
      gfx::ImageSkia::CreateFromBitmap(background_screenshot_, /*scale=*/1.f);

  canvas->DrawImageInt(
      image, /*src_x=*/0, /*src_y=*/0, /*src_w=*/background_screenshot_.width(),
      /*src_h=*/background_screenshot_.height(), /*dest_x=*/0, /*dest_y=*/0,
      /*_dest_w_=*/layer_size.width(), /*dest_h=*/layer_size.height(),
      /*filter=*/false, filter_flags);
}

void LensOverlayBlurLayerDelegate::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  // Do nothing. OnPaintLayer will automatically repaint with a new image.
}

void LensOverlayBlurLayerDelegate::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  CHECK(widget_host == background_view_host_);
  render_widget_host_observer_.Reset();
  background_view_host_ = nullptr;
  // If the host view was destroyed, stop updating the background blur.
  StopBackgroundImageCapture();
}

void LensOverlayBlurLayerDelegate::FetchBackgroundImage() {
  if (!background_view_host_ || !background_view_host_->GetView()) {
    return;
  }

  auto* view = background_view_host_->GetView();
  auto size = view->GetViewBounds().size();
  auto quality = lens::features::GetLensOverlayCustomBlurQuality();
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(),
      /*output_size=*/
      gfx::Size(size.width() * quality, size.height() * quality),
      base::BindOnce(&LensOverlayBlurLayerDelegate::UpdateBackgroundImage,
                     weak_factory_.GetWeakPtr()));
}

void LensOverlayBlurLayerDelegate::UpdateBackgroundImage(
    const viz::CopyOutputBitmapWithMetadata& result) {
  const auto& bitmap = result.bitmap;
  auto layer_size = layer()->size();
  if (bitmap.drawsNothing() || layer_size.width() * layer_size.height() <= 0 ||
      gfx::BitmapsAreEqual(background_screenshot_, bitmap)) {
    return;
  }
  background_screenshot_ = bitmap;
  layer()->SchedulePaint(gfx::Rect(layer_size));
}

}  // namespace lens
