// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/qrcode_generator/qrcode_generator_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "components/qr_code_generator/dino_image.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/paint_vector_icon.h"

namespace qrcode_generator {

// Allow each element to render as this many pixels.
static const int kModuleSizePixels = 10;

// Allow each dino tile to render as this many pixels.
static const int kDinoTileSizePixels = 4;

// Size of a QR locator, in modules.
static const int kLocatorSizeModules = 7;

QRCodeGeneratorServiceImpl::QRCodeGeneratorServiceImpl(
    mojo::PendingReceiver<mojom::QRCodeGeneratorService> receiver)
    : receiver_(this, std::move(receiver)) {
  InitializeDinoBitmap();
}

QRCodeGeneratorServiceImpl::~QRCodeGeneratorServiceImpl() = default;

void QRCodeGeneratorServiceImpl::InitializeDinoBitmap() {
  // The dino is taller than it is wide; validate this assumption in debug
  // builds to simplify some calculations later.
  DCHECK_GE(dino_image::kDinoHeight, dino_image::kDinoWidth);

  dino_bitmap_.allocN32Pixels(dino_image::kDinoWidth, dino_image::kDinoHeight);
  dino_bitmap_.eraseARGB(0xFF, 0xFF, 0xFF, 0xFF);
  SkCanvas canvas(dino_bitmap_, SkSurfaceProps{});
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
}

void QRCodeGeneratorServiceImpl::DrawPasskeyIcon(
    SkCanvas* canvas,
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

void QRCodeGeneratorServiceImpl::DrawDino(SkCanvas* canvas,
                                          const SkRect& canvas_bounds,
                                          const int pixels_per_dino_tile,
                                          const int dino_border_px,
                                          const SkPaint& paint_foreground,
                                          const SkPaint& paint_background) {
  int dino_width_px = pixels_per_dino_tile * dino_image::kDinoWidth;
  int dino_height_px = pixels_per_dino_tile * dino_image::kDinoHeight;
  PaintCenterImage(canvas, canvas_bounds, dino_width_px, dino_height_px,
                   dino_border_px, paint_background, dino_bitmap_);
}

void QRCodeGeneratorServiceImpl::PaintCenterImage(
    SkCanvas* canvas,
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
static void DrawLocators(SkCanvas* canvas,
                         const gfx::Size data_size,
                         const SkPaint& paint_foreground,
                         const SkPaint& paint_background,
                         mojom::LocatorStyle style) {
  SkScalar radius = style == mojom::LocatorStyle::ROUNDED ? 10 : 0;

  // Draw a locator with upper left corner at {x, y} in terms of module
  // coordinates.
  auto drawOneLocator = [&](int left_x_modules, int top_y_modules) {
    // Outermost square, 7x7 modules.
    int left_x_pixels = left_x_modules * kModuleSizePixels;
    int top_y_pixels = top_y_modules * kModuleSizePixels;
    int dim_pixels = kModuleSizePixels * kLocatorSizeModules;
    canvas->drawRoundRect(
        gfx::RectToSkRect(
            gfx::Rect(left_x_pixels, top_y_pixels, dim_pixels, dim_pixels)),
        radius, radius, paint_foreground);
    // Middle square, one module smaller in all dimensions (5x5).
    left_x_pixels += kModuleSizePixels;
    top_y_pixels += kModuleSizePixels;
    dim_pixels -= 2 * kModuleSizePixels;
    canvas->drawRoundRect(
        gfx::RectToSkRect(
            gfx::Rect(left_x_pixels, top_y_pixels, dim_pixels, dim_pixels)),
        radius, radius, paint_background);
    // Inner square, one additional module smaller in all dimensions (3x3).
    left_x_pixels += kModuleSizePixels;
    top_y_pixels += kModuleSizePixels;
    dim_pixels -= 2 * kModuleSizePixels;
    canvas->drawRoundRect(
        gfx::RectToSkRect(
            gfx::Rect(left_x_pixels, top_y_pixels, dim_pixels, dim_pixels)),
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

void QRCodeGeneratorServiceImpl::RenderBitmap(
    base::span<const uint8_t> data,
    const gfx::Size data_size,
    const mojom::GenerateQRCodeRequestPtr& request,
    mojom::GenerateQRCodeResponsePtr* response) {
  if (!request->should_render)
    return;

  // Setup: create colors and clear canvas.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(data_size.width() * kModuleSizePixels,
                        data_size.height() * kModuleSizePixels);
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

        if (request->render_module_style == mojom::ModuleStyle::CIRCLES) {
          float xc = (x + 0.5) * kModuleSizePixels;
          float yc = (y + 0.5) * kModuleSizePixels;
          SkScalar radius = kModuleSizePixels / 2 - 1;
          canvas.drawCircle(xc, yc, radius, paint_black);
        } else {
          canvas.drawRect(gfx::RectToSkRect(gfx::Rect(
                              x * kModuleSizePixels, y * kModuleSizePixels,
                              kModuleSizePixels, kModuleSizePixels)),
                          paint_black);
        }
      }
    }
  }

  DrawLocators(&canvas, data_size, paint_black, paint_white,
               request->render_locator_style);

  SkRect bitmap_bounds;
  bitmap.getBounds(&bitmap_bounds);

  switch (request->center_image) {
    case mojom::CenterImage::DEFAULT_NONE:
      break;
    case mojom::CenterImage::CHROME_DINO:
      DrawDino(&canvas, bitmap_bounds, kDinoTileSizePixels, 2, paint_black,
               paint_white);
      break;
    case mojom::CenterImage::PASSKEY_ICON:
      DrawPasskeyIcon(&canvas, bitmap_bounds, paint_black, paint_white);
      break;
  }

  (*response)->bitmap = bitmap;
}

void QRCodeGeneratorServiceImpl::GenerateQRCode(
    mojom::GenerateQRCodeRequestPtr request,
    GenerateQRCodeCallback callback) {
  mojom::GenerateQRCodeResponsePtr response =
      mojom::GenerateQRCodeResponse::New();

  if (!request->data.data()) {
    response->error_code = mojom::QRCodeGeneratorError::UNKNOWN_ERROR;
    std::move(callback).Run(std::move(response));
    return;
  }

  // TODO(lukasza): Consider increasing `kLengthMax` - according to
  // https://www.qrcode.com/en/about/version.html 177x177 QR code can encode up
  // to 7089 digits.
  constexpr size_t kLengthMax = 288;
  if (request->data.length() > kLengthMax) {
    response->error_code = mojom::QRCodeGeneratorError::INPUT_TOO_LONG;
    std::move(callback).Run(std::move(response));
    return;
  }

  QRCodeGenerator qr;
  // The QR version (i.e. size) must be >= 5 because otherwise the dino painted
  // over the middle covers too much of the code to be decodable.
  constexpr int kMinimumQRVersion = 5;
  absl::optional<QRCodeGenerator::GeneratedCode> qr_data =
      qr.Generate(base::span<const uint8_t>(
                      reinterpret_cast<const uint8_t*>(request->data.data()),
                      request->data.size()),
                  kMinimumQRVersion);
  if (!qr_data || qr_data->data.data() == nullptr ||
      qr_data->data.size() == 0) {
    // The above check should have caught the too-long-URL case.
    // Remaining errors can be treated as UNKNOWN.
    response->error_code = mojom::QRCodeGeneratorError::UNKNOWN_ERROR;
    std::move(callback).Run(std::move(response));
    return;
  }
  // The least significant bit of each byte in |qr_data.span| is set if the tile
  // should be black.
  for (uint8_t& byte : qr_data->data) {
    byte &= 1;
  }

  response->data = std::move(qr_data->data);
  response->data_size = {qr_data->qr_size, qr_data->qr_size};
  response->error_code = mojom::QRCodeGeneratorError::NONE;
  RenderBitmap(base::make_span(response->data), response->data_size, request,
               &response);

  std::move(callback).Run(std::move(response));
}

}  // namespace qrcode_generator
