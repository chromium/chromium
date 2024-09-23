// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/clients/blur.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "ui/gl/gl_bindings.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

// Rotation speed (degrees/second).
const double kRotationSpeed = 360.0;

// The opacity of the foreground.
const double kForegroundOpacity = 0.7;

// Rects grid size.
const int kGridSize = 4;

// Create grid image for |size| and |cell_size|.
sk_sp<SkImage> CreateGridImage(const gfx::Size& size,
                               const gfx::Size& cell_size) {
  sk_sp<SkSurface> surface(SkSurfaces::Raster(
      SkImageInfo::MakeN32(size.width(), size.height(), kOpaque_SkAlphaType)));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);
  for (int y = 0; y < kGridSize; ++y) {
    for (int x = 0; x < kGridSize; ++x) {
      if ((y + x) % 2)
        continue;
      SkPaint paint;
      paint.setColor(SK_ColorLTGRAY);
      canvas->save();
      canvas->translate(x * cell_size.width(), y * cell_size.height());
      canvas->drawIRect(SkIRect::MakeWH(cell_size.width(), cell_size.height()),
                        paint);
      canvas->restore();
    }
  }
  return surface->makeImageSnapshot();
}

// Adjust sigma by increasing the scale factor until less than |max_sigma|.
// Returns the adjusted sigma value.
double AdjustSigma(double sigma, double max_sigma, int* scale_factor) {
  *scale_factor = 1;
  while (sigma > max_sigma) {
    *scale_factor *= 2;
    sigma /= 2.0;
  }
  return sigma;
}

// Draw background contents to canvas.
void DrawContents(SkImage* background_grid_image,
                  const gfx::Size& cell_size,
                  base::TimeDelta elapsed_time,
                  SkCanvas* canvas) {
  // Draw background grid.
  {
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas->drawImage(background_grid_image, 0, 0, SkSamplingOptions(), &paint);
  }

  // Draw rotated rectangles.
  SkScalar rect_size =
      SkScalarHalf(std::min(cell_size.width(), cell_size.height()));
  SkIRect rect = SkIRect::MakeXYWH(
      -SkScalarHalf(rect_size), -SkScalarHalf(rect_size), rect_size, rect_size);
  SkScalar rotation = elapsed_time.InMilliseconds() * kRotationSpeed / 1000;
  for (int y = 0; y < kGridSize; ++y) {
    for (int x = 0; x < kGridSize; ++x) {
      const SkColor kColors[] = {SK_ColorBLUE, SK_ColorGREEN,
                                 SK_ColorRED,  SK_ColorYELLOW,
                                 SK_ColorCYAN, SK_ColorMAGENTA};
      SkPaint paint;
      paint.setColor(kColors[(y * kGridSize + x) % std::size(kColors)]);
      canvas->save();
      canvas->translate(
          x * cell_size.width() + SkScalarHalf(cell_size.width()),
          y * cell_size.height() + SkScalarHalf(cell_size.height()));
      canvas->rotate(rotation / (y * kGridSize + x + 1));
      canvas->drawIRect(rect, paint);
      canvas->restore();
    }
  }
}

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

}  // namespace

Blur::Blur() = default;

Blur::~Blur() = default;

void Blur::Run(double sigma_x,
               double sigma_y,
               double max_sigma,
               bool offscreen,
               int frames) {
  Buffer* buffer = buffers_.front().get();
  gfx::Size cell_size(size_.width() / kGridSize, size_.height() / kGridSize);

  // Create grid image. Simulates a wallpaper.
  if (!grid_image_)
    grid_image_ = CreateGridImage(size_, cell_size);

  // Create blur surfaces if needed.
  sk_sp<SkImageFilter> blur_filter;
  std::vector<sk_sp<SkSurface>> blur_surfaces;
  std::vector<sk_sp<SkSurface>> content_surfaces;
  int blur_scale_factor_x = 1;
  int blur_scale_factor_y = 1;
  if (sigma_x > 0.0 || sigma_y > 0.0) {
    sigma_x = AdjustSigma(sigma_x, max_sigma, &blur_scale_factor_x);
    sigma_y = AdjustSigma(sigma_y, max_sigma, &blur_scale_factor_y);
    blur_filter = SkImageFilters::Blur(sigma_x, sigma_y, SkTileMode::kClamp,
                                       nullptr, nullptr);
    auto size = SkISize::Make(size_.width() / blur_scale_factor_x,
                              size_.height() / blur_scale_factor_y);
    do {
      blur_surfaces.push_back(
          buffer->sk_surface->makeSurface(SkImageInfo::MakeN32(
              size.width(), size.height(), kOpaque_SkAlphaType)));
      size = SkISize::Make(std::min(size_.width(), size.width() * 2),
                           std::min(size_.height(), size.height() * 2));
    } while (size.width() < size_.width() || size.height() < size_.height());
  }

  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};
  int frame_count = 0;
  base::TimeTicks initial_time = base::TimeTicks::Now();
  while (frame_count < frames) {
    base::TimeDelta elapsed_time = base::TimeTicks::Now() - initial_time;
    if (blur_filter) {
      // Create contents surfaces.
      while (!blur_surfaces.empty()) {
        sk_sp<SkSurface> surface = blur_surfaces.back();
        blur_surfaces.pop_back();

        SkSize size = SkSize::Make(surface->width(), surface->height());
        SkCanvas* canvas = surface->getCanvas();
        canvas->save();

        // Draw contents if this is the first surface.
        if (content_surfaces.empty()) {
          canvas->scale(size.width() / size_.width(),
                        size.height() / size_.height());
          DrawContents(grid_image_.get(), cell_size, elapsed_time, canvas);
        } else {
          // Otherwise, scale larger surface to produce surface.
          canvas->scale(size.width() / content_surfaces.back()->width(),
                        size.height() / content_surfaces.back()->height());
          SkPaint paint;
          paint.setBlendMode(SkBlendMode::kSrc);
          content_surfaces.back()->draw(
              canvas, 0, 0, SkSamplingOptions(SkFilterMode::kLinear), &paint);
        }

        canvas->restore();
        content_surfaces.push_back(surface);
      }

      // Make blur image from last content surface.
      SkIRect subset;
      SkIPoint offset;
      sk_sp<SkImage> blur_image = content_surfaces.back()->makeImageSnapshot();

      sk_sp<SkImage> blurred_image;
      if (gr_context_.get()) {
        blurred_image = SkImages::MakeWithFilter(
            gr_context_.get(), blur_image, blur_filter.get(), blur_image->bounds(),
            blur_image->bounds(), &subset, &offset);
      } else {
        blurred_image =
            SkImages::MakeWithFilter(blur_image, blur_filter.get(), blur_image->bounds(),
                                     blur_image->bounds(), &subset, &offset);
      }

      SkCanvas* canvas = buffer->sk_surface->getCanvas();
      canvas->save();
      SkSize size = SkSize::Make(size_.width(), size_.height());
      canvas->scale(size.width() / blur_image->width(),
                    size.height() / blur_image->height());
      SkPaint paint;
      paint.setBlendMode(SkBlendMode::kSrc);
      SkSamplingOptions sampling(SkFilterMode::kLinear);
      // Simulate multi-texturing by adding foreground opacity.
      int alpha = (1.0 - kForegroundOpacity) * 255.0 + 0.5;
      paint.setColor(SkColorSetA(SK_ColorBLACK, alpha));
      canvas->drawImage(blurred_image, offset.x() - subset.x(),
                        offset.y() - subset.y(), sampling, &paint);
      canvas->restore();

      // Restore blur surfaces for next frame.
      std::swap(content_surfaces, blur_surfaces);
      std::reverse(blur_surfaces.begin(), blur_surfaces.end());
    } else {  // !blur_filter
      SkCanvas* canvas = buffer->sk_surface->getCanvas();
      DrawContents(grid_image_.get(), cell_size, elapsed_time, canvas);
      SkPaint paint;
      int alpha = kForegroundOpacity * 255.0 + 0.5;
      paint.setColor(SkColorSetA(SK_ColorBLACK, alpha));
      canvas->drawIRect(SkIRect::MakeWH(size_.width(), size_.height()), paint);
    }

    if (gr_context_) {
      gr_context_->flushAndSubmit();
      glFinish();
    }

    // Submit 1 of 50 frames for onscreen display when in offscreen mode.
    if ((frame_count++ % 50) && offscreen)
      continue;

    while (callback_pending)
      wl_display_dispatch(display_.get());

    callback_pending = true;

    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &callback_pending);
    wl_surface_commit(surface_.get());
    wl_display_flush(display_.get());
  }
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo
