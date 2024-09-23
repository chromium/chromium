// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/qr_code_generator/bitmap_generator.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/qr_code_generator/dino_image.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/paint_vector_icon.h"

namespace qr_code_generator {

namespace {

// Allow each element to render as this many pixels.
static const int kModuleSizePixels = 10;

// Allow each dino tile to render as this many pixels.
static const int kDinoTileSizePixels = 4;

// Size of a QR locator, in modules.
static const int kLocatorSizeModules = 7;

SkBitmap CreateDinoBitmap() {
  // The dino is taller than it is wide; validate this assumption in debug
  // builds to simplify some calculations later.
  DCHECK_GE(dino_image::kDinoHeight, dino_image::kDinoWidth);

  SkBitmap dino_bitmap;
  dino_bitmap.allocN32Pixels(dino_image::kDinoWidth, dino_image::kDinoHeight);
  dino_bitmap.eraseARGB(0xFF, 0xFF, 0xFF, 0xFF);
  SkCanvas canvas(dino_bitmap, SkSurfaceProps{});
  SkPaint paint;
  paint.setColor(SK_ColorBLACK);

  constexpr int bytes_per_row = (dino_image::kDinoHeight + 7) / 8;

  // Helper: Copies |src_num_rows| of dino data from |src_array| to
  // canvas (obtained via closure), starting at |dest_row|.
  auto copyPixelBitData = [&](const unsigned char* src_array, int src_num_rows,
                              int dest_row) {
    for (int row = 0; row < src_num_rows; row++) {
      int which_byte = (row * bytes_per_row);
      unsigned char mask = 0b10000000;
      for (int col = 0; col < dino_image::kDinoWidth; col++) {
        if (*(src_array + which_byte) & mask) {
          canvas.drawIRect({col, dest_row + row, col + 1, dest_row + row + 1},
                           paint);
        }
        mask >>= 1;
        if (mask == 0) {
          mask = 0b10000000;
          which_byte++;
        }
      }
    }
  };

  copyPixelBitData(dino_image::kDinoHeadRight, dino_image::kDinoHeadHeight, 0);
  copyPixelBitData(dino_image::kDinoBody, dino_image::kDinoBodyHeight,
                   dino_image::kDinoHeadHeight);

  return dino_bitmap;
}

void PaintCenterImage(SkCanvas* canvas,
                      const SkRect& canvas_bounds,
                      const int width_px,
                      const int height_px,
                      const int border_px,
                      const SkPaint& paint_background,
                      const SkBitmap& image);

// `gfx::CreateVectorIcon` is not available on iOS.
#if !BUILDFLAG(IS_IOS)
void DrawPasskeyIcon(SkCanvas* canvas,
                     const SkRect& canvas_bounds,
                     const SkPaint& paint_foreground,
                     const SkPaint& paint_background) {
  constexpr int kSizePx = 100;
  constexpr int kBorderPx = 0;  // Unlike the dino, the icon is already padded.
  auto icon = gfx::CreateVectorIcon(gfx::IconDescription(
      vector_icons::kPasskeyIcon, kSizePx, paint_foreground.getColor()));
  PaintCenterImage(canvas, canvas_bounds, kSizePx, kSizePx, kBorderPx,
                   paint_background, icon.GetRepresentation(1.0f).GetBitmap());
}

void DrawProductIcon(SkCanvas* canvas,
                     const SkRect& canvas_bounds,
                     const SkPaint& paint_foreground,
                     const SkPaint& paint_background) {
  constexpr int kSizePx = 100;
  constexpr int kBorderPx = 0;  // Unlike the dino, the icon is already padded.
  auto icon = gfx::CreateVectorIcon(gfx::IconDescription(
      vector_icons::kProductRefreshIcon, kSizePx, paint_foreground.getColor()));
  PaintCenterImage(canvas, canvas_bounds, kSizePx, kSizePx, kBorderPx,
                   paint_background, icon.GetRepresentation(1.0f).GetBitmap());
}
#endif

void DrawDino(SkCanvas* canvas,
              const SkRect& canvas_bounds,
              const int pixels_per_dino_tile,
              const int dino_border_px,
              const SkPaint& paint_foreground,
              const SkPaint& paint_background) {
  SkBitmap dino_bitmap = CreateDinoBitmap();
  int dino_width_px = pixels_per_dino_tile * dino_image::kDinoWidth;
  int dino_height_px = pixels_per_dino_tile * dino_image::kDinoHeight;
  PaintCenterImage(canvas, canvas_bounds, dino_width_px, dino_height_px,
                   dino_border_px, paint_background, dino_bitmap);
}

void PaintCenterImage(SkCanvas* canvas,
                      const SkRect& canvas_bounds,
                      const int width_px,
                      const int height_px,
                      const int border_px,
                      const SkPaint& paint_background,
                      const SkBitmap& image) {
  // If we request too big an image, we'll clip. In practice the image size
  // should be significantly smaller than the canvas to leave room for the
  // data payload and locators, so alert if we take over 25% of the area.
  DCHECK_GE(canvas_bounds.width() / 2, width_px + border_px);
  DCHECK_GE(canvas_bounds.height() / 2, height_px + border_px);

  // Assemble the target rect for the dino image data.
  SkRect dest_rect = SkRect::MakeWH(width_px, height_px);
  dest_rect.offset((canvas_bounds.width() - dest_rect.width()) / 2,
                   (canvas_bounds.height() - dest_rect.height()) / 2);

  // Clear out a little room for a border, snapped to some number of modules.
  SkRect background = SkRect::MakeLTRB(
      std::floor((dest_rect.left() - border_px) / kModuleSizePixels) *
          kModuleSizePixels,
      std::floor((dest_rect.top() - border_px) / kModuleSizePixels) *
          kModuleSizePixels,
      std::floor((dest_rect.right() + border_px + kModuleSizePixels - 1) /
                 kModuleSizePixels) *
          kModuleSizePixels,
      std::floor((dest_rect.bottom() + border_px + kModuleSizePixels - 1) /
                 kModuleSizePixels) *
          kModuleSizePixels);
  canvas->drawRect(background, paint_background);

  // Center the image within the cleared space, and draw it.
  SkScalar delta_x =
      SkScalarRoundToScalar(background.centerX() - dest_rect.centerX());
  SkScalar delta_y =
      SkScalarRoundToScalar(background.centerY() - dest_rect.centerY());
  dest_rect.offset(delta_x, delta_y);
  SkRect image_bounds;
  image.getBounds(&image_bounds);
  canvas->drawImageRect(image.asImage(), image_bounds, dest_rect,
                        SkSamplingOptions(), nullptr,
                        SkCanvas::kStrict_SrcRectConstraint);
}

// Draws QR locators at three corners of |canvas|.
void DrawLocators(SkCanvas* canvas,
                  const gfx::Size data_size,
                  const SkPaint& paint_foreground,
                  const SkPaint& paint_background,
                  LocatorStyle style,
                  int margin) {
  SkScalar radius = style == LocatorStyle::kRounded ? 10 : 0;

  // Draw a locator with upper left corner at {x, y} in terms of module
  // coordinates.
  auto drawOneLocator = [&](int left_x_modules, int top_y_modules) {
    // Outermost square, 7x7 modules.
    int left_x_pixels = left_x_modules * kModuleSizePixels;
    int top_y_pixels = top_y_modules * kModuleSizePixels;
    int dim_pixels = kModuleSizePixels * kLocatorSizeModules;
    canvas->drawRoundRect(gfx::RectToSkRect(gfx::Rect(margin + left_x_pixels,
                                                      margin + top_y_pixels,
                                                      dim_pixels, dim_pixels)),
                          radius, radius, paint_foreground);
    // Middle square, one module smaller in all dimensions (5x5).
    left_x_pixels += kModuleSizePixels;
    top_y_pixels += kModuleSizePixels;
    dim_pixels -= 2 * kModuleSizePixels;
    canvas->drawRoundRect(gfx::RectToSkRect(gfx::Rect(margin + left_x_pixels,
                                                      margin + top_y_pixels,
                                                      dim_pixels, dim_pixels)),
                          radius, radius, paint_background);
    // Inner square, one additional module smaller in all dimensions (3x3).
    left_x_pixels += kModuleSizePixels;
    top_y_pixels += kModuleSizePixels;
    dim_pixels -= 2 * kModuleSizePixels;
    canvas->drawRoundRect(gfx::RectToSkRect(gfx::Rect(margin + left_x_pixels,
                                                      margin + top_y_pixels,
                                                      dim_pixels, dim_pixels)),
                          radius, radius, paint_foreground);
  };

  // Top-left
  drawOneLocator(0, 0);
  // Top-right
  drawOneLocator(data_size.width() - kLocatorSizeModules, 0);
  // Bottom-left
  drawOneLocator(0, data_size.height() - kLocatorSizeModules);
  // No locator on bottom-right.
}

int CalculateMargin(QuietZone quiet_zone) {
  switch (quiet_zone) {
    case QuietZone::kIncluded:
      return kQuietZoneSizePixels;
    case QuietZone::kWillBeAddedByClient:
      return 0;
  }
  NOTREACHED();
}

SkBitmap RenderBitmap(base::span<const uint8_t> data,
                      const gfx::Size data_size,
                      ModuleStyle module_style,
                      LocatorStyle locator_style,
                      CenterImage center_image,
                      QuietZone quiet_zone) {
  // Setup: create colors and clear canvas.
  SkBitmap bitmap;
  int margin = CalculateMargin(quiet_zone);
  bitmap.allocN32Pixels(data_size.width() * kModuleSizePixels + margin * 2,
                        data_size.height() * kModuleSizePixels + margin * 2);
  bitmap.eraseARGB(0xFF, 0xFF, 0xFF, 0xFF);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  SkPaint paint_black;
  paint_black.setColor(SK_ColorBLACK);
  SkPaint paint_white;
  paint_white.setColor(SK_ColorWHITE);

  // Loop over qr module data and paint to canvas.
  // Paint data modules first, then locators and dino.
  int data_index = 0;
  for (int y = 0; y < data_size.height(); y++) {
    for (int x = 0; x < data_size.width(); x++) {
      if (data[data_index++] & 0x1) {
        bool is_locator =
            (y <= kLocatorSizeModules &&
             (x <= kLocatorSizeModules ||
              x >= data_size.width() - kLocatorSizeModules - 1)) ||
            (y >= data_size.height() - kLocatorSizeModules - 1 &&
             x <= kLocatorSizeModules);
        if (is_locator) {
          continue;
        }

        if (module_style == ModuleStyle::kCircles) {
          float xc = margin + (x + 0.5) * kModuleSizePixels;
          float yc = margin + (y + 0.5) * kModuleSizePixels;
          SkScalar radius = kModuleSizePixels / 2 - 1;
          canvas.drawCircle(xc, yc, radius, paint_black);
        } else {
          int x0 = margin + x * kModuleSizePixels;
          int y0 = margin + y * kModuleSizePixels;
          const int kRectSize = kModuleSizePixels;
          SkRect rect = gfx::RectToSkRect({x0, y0, kRectSize, kRectSize});
          canvas.drawRect(rect, paint_black);
        }
      }
    }
  }

  DrawLocators(&canvas, data_size, paint_black, paint_white, locator_style,
               margin);

  SkRect bitmap_bounds;
  bitmap.getBounds(&bitmap_bounds);

  switch (center_image) {
    case CenterImage::kNoCenterImage:
      break;
    case CenterImage::kDino:
      DrawDino(&canvas, bitmap_bounds, kDinoTileSizePixels, 2, paint_black,
               paint_white);
      break;
#if !BUILDFLAG(IS_IOS)
    case CenterImage::kPasskey:
      DrawPasskeyIcon(&canvas, bitmap_bounds, paint_black, paint_white);
      break;
    case CenterImage::kProductLogo:
      DrawProductIcon(&canvas, bitmap_bounds, paint_black, paint_white);
      break;
#endif
  }

  return bitmap;
}

}  // namespace

const int kQuietZoneSizePixels = kModuleSizePixels * 4;

base::expected<gfx::ImageSkia, Error> GenerateImage(
    base::span<const uint8_t> data,
    ModuleStyle module_style,
    LocatorStyle locator_style,
    CenterImage center_image,
    QuietZone quiet_zone) {
  // TODO(crbug.com/338570710) CreateImage() should generate a higher resolution
  // QR code for displays with scale-factor > 1. Not generating higher
  // resolution QR codes is OK because:
  // - QR codes are shown to the user rarely.
  // - Many callers display the QR code at a downsampled size.
  // - Upscaling QR codes has few upscaling artifacts.
  return GenerateBitmap(data, module_style, locator_style, center_image,
                        quiet_zone)
      .transform(&gfx::ImageSkia::CreateFrom1xBitmap);
}

base::expected<SkBitmap, Error> GenerateBitmap(base::span<const uint8_t> data,
                                               ModuleStyle module_style,
                                               LocatorStyle locator_style,
                                               CenterImage center_image,
                                               QuietZone quiet_zone) {
  SCOPED_UMA_HISTOGRAM_TIMER("Sharing.QRCodeGeneration.Duration");

  GeneratedCode qr_code;
  {
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Sharing.QRCodeGeneration.Duration.BytesToQrPixels2");

    // The QR version (i.e. size) must be >= 5 because otherwise the dino
    // painted over the middle covers too much of the code to be decodable.
    constexpr int kMinimumQRVersion = 5;
    auto qr_result = GenerateCode(data, kMinimumQRVersion);
    if (!qr_result.has_value()) {
      return base::unexpected(qr_result.error());
    }
    qr_code = std::move(qr_result.value());
  }

  // The least significant bit of each byte in |qr_data.span| is set if the tile
  // should be black.
  for (uint8_t& byte : qr_code.data) {
    byte &= 1;
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Sharing.QRCodeGeneration.Duration.QrPixelsToQrImage2");
    gfx::Size data_size = {qr_code.qr_size, qr_code.qr_size};
    return RenderBitmap(base::make_span(qr_code.data), data_size, module_style,
                        locator_style, center_image, quiet_zone);
  }
}

}  // namespace qr_code_generator
