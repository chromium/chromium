// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"

#import <Cocoa/Cocoa.h>

#include <array>
#include <queue>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "third_party/skia/src/core/SkBitmapDevice.h"
#include "third_party/skia/src/core/SkDraw.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"

namespace {

// Returns parameters for Apple's icon grid bounding box and shadow for the
// given size. Values are derived from Apple icon design templates (March 2023
// macOS Production Templates).
struct IconGridParameters {
  SkScalar inset;
  SkScalar corner_radius;
  SkScalar shadow_y_offset;
  SkScalar shadow_blur_radius;
};

IconGridParameters GetIconGridParameters(int base_size) {
  // According to Apple design templates, a macOS icon should be a rounded
  // rect surrounded by some transparent padding.  The rounded rect's size
  // is approximately 80% of the overall icon, but this percentage varies.
  // Exact mask size and shape gleaned from Apple icon design templates,
  // specifically the March 2023 macOS Production Templates Sketch file at
  // https://developer.apple.com/design/resources/.  A few corner radius
  // values were unavailable in the file because the relevant shapes were
  // represenated as plain paths rather than rounded rects.
  //
  // The Web App Manifest spec defines a safe zone for maskable icons
  // (https://www.w3.org/TR/appmanifest/#icon-masks) in a centered circle
  // with diameter 80% of the overall icon.  Since the macOS icon mask
  // is a rounded rect that is never smaller than 80% of the overall icon,
  // it is within spec to simply draw our base icon full size and clip
  // whatever is outside of the rounded rect.  This is what is currently
  // implemented, even though is is different from what Apple does in macOS
  // Sonoma web apps (where instead they first scale the icon to cover just
  // the rounded rect, only clipping the corners).  Somewhere in the middle
  // of these options might be ideal, although with the current icon loading
  // code icons have already been resized to neatly fill entire standard sized
  // icons by the time this code runs, so doing any more resizing here would
  // not be great.
  switch (base_size) {
    case 16:
      // An exact value for the 16 corner radius was not available.
      // this corner radius is derived by dividing the 32 radius by 2
      return {.inset = 1.0,
              .corner_radius = 2.785,
              .shadow_y_offset = 0.5,
              .shadow_blur_radius = 0.5};
    case 32:
      return {.inset = 2.0,
              .corner_radius = 5.75,
              .shadow_y_offset = 1.0,
              .shadow_blur_radius = 1.0};
    case 64:
      return {.inset = 6.0,
              .corner_radius = 11.5,
              .shadow_y_offset = 2.0,
              .shadow_blur_radius = 2.0};
    case 128:
      // An exact value for the 128 corner radius was not available.
      // this corner radius is derived by dividing the 256 radius by 2
      // or by multiplying the 64 radius by 2, both calculations
      // have the same result.
      return {.inset = 12.0,
              .corner_radius = 23.0,
              .shadow_y_offset = 1.25,
              .shadow_blur_radius = 1.25};
    case 256:
      return {.inset = 25.0,
              .corner_radius = 46.0,
              .shadow_y_offset = 2.5,
              .shadow_blur_radius = 2.5};
    case 512:
      return {.inset = 50.0,
              .corner_radius = 92.0,
              .shadow_y_offset = 5.0,
              .shadow_blur_radius = 5.0};
    case 1024:
      // An exact value for the 1024 corner radius was not available.
      // this corner radius is derived by multiplying the 512 radius by 2
      return {.inset = 100.0,
              .corner_radius = 184.0,
              .shadow_y_offset = 10.0,
              .shadow_blur_radius = 10.0};
    default:
      // Use 1024 sizes as a reference for approximating any size mask if needed
      return {
          .inset = static_cast<SkScalar>(base_size * 100.0 / 1024.0),
          .corner_radius = static_cast<SkScalar>(base_size * 184.0 / 1024.0),
          .shadow_y_offset = static_cast<SkScalar>(base_size * 10.0 / 1024.0),
          .shadow_blur_radius =
              static_cast<SkScalar>(base_size * 10.0 / 1024.0)};
  }
}

SkPath CreateMaskingIconGridBoundingBoxPath(int base_size,
                                            const IconGridParameters& params) {
  SkRect rect = SkRect::MakeIWH(base_size, base_size)
                    .makeInset(params.inset, params.inset);
  return SkPath::RRect(rect, params.corner_radius, params.corner_radius);
}

// Scales the given icon down to fit appropriately within the masked area.
gfx::Image ScaleDownInsideMask(const gfx::Image& icon,
                               IconGridParameters grid_params) {
  const int width = icon.Width();
  // Always scale the icon to 10% of the mask area - this means the corners may
  // be slightly clipped, but this looks good, and scaling down further is too
  // small.
  int masked_width = width - 2 * grid_params.inset;
  int margin = grid_params.inset + static_cast<int>(masked_width * 0.1f);
  gfx::Canvas canvas(gfx::Size(width, width), 1.0f, /*is_opaque=*/false);
  canvas.sk_canvas()->clear(SkColor4f{0, 0, 0, 0});

  const gfx::RectF dst_rect(margin, margin, width - 2 * margin,
                            width - 2 * margin);

  canvas.DrawImageInt(icon.AsImageSkia(),
                      /*src_x=*/0, /*src_y=*/0, width, width, dst_rect.x(),
                      dst_rect.y(), dst_rect.width(), dst_rect.height(),
                      /*filter=*/true);

  return gfx::Image::CreateFrom1xBitmap(canvas.GetBitmap());
}

// Checks that all pixels outside of the given mask are within
// `max_non_alpha_deviation`. All pixels less than `min_alpha` are ignored to
// help prevent false negatives.
bool AreColorsOutsideMaskWithinDeviation(const SkBitmap& icon,
                                         const SkPath& mask_path,
                                         uint8_t min_alpha,
                                         int max_non_alpha_deviation) {
  if (!icon.peekPixels(nullptr)) {
    return false;
  }

  std::array<uint8_t, 3> min_components = {255, 255, 255};  // R, G, B
  std::array<uint8_t, 3> max_components = {0, 0, 0};

  SkPixmap pixmap;
  icon.peekPixels(&pixmap);

  for (int x = 0; x < icon.width(); ++x) {
    for (int y = 0; y < icon.height(); ++y) {
      if (mask_path.contains(SkIntToScalar(x), SkIntToScalar(y))) {
        continue;  // Skip pixels inside the mask
      }

      SkColor current_color = pixmap.getColor(x, y);

      if (SkColorGetA(current_color) < min_alpha) {
        continue;
      }

      min_components[0] =
          std::min<uint8_t>(min_components[0], SkColorGetR(current_color));
      min_components[1] =
          std::min<uint8_t>(min_components[1], SkColorGetG(current_color));
      min_components[2] =
          std::min<uint8_t>(min_components[2], SkColorGetB(current_color));

      max_components[0] =
          std::max<uint8_t>(max_components[0], SkColorGetR(current_color));
      max_components[1] =
          std::max<uint8_t>(max_components[1], SkColorGetG(current_color));
      max_components[2] =
          std::max<uint8_t>(max_components[2], SkColorGetB(current_color));

      for (size_t i = 0; i < 3; ++i) {
        if (max_components[i] - min_components[i] > max_non_alpha_deviation) {
          return false;
        }
      }
    }
  }

  return true;
}

gfx::Image CreateAppleMaskedAppIconWithPath(
    const gfx::Image& base_icon,
    const SkPath& icon_grid_bounding_box_path) {
  int base_size = base_icon.Width();
  SkImageInfo info =
      SkImageInfo::Make(base_size, base_size, SkColorType::kN32_SkColorType,
                        SkAlphaType::kUnpremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  SkCanvas canvas(bitmap);
  SkRect base_rect = SkRect::MakeIWH(base_size, base_size);

  // Draw the shadow
  SkPaint shadowPaint;
  shadowPaint.setStyle(SkPaint::kFill_Style);
  shadowPaint.setARGB(77, 0, 0, 0);
  IconGridParameters params = GetIconGridParameters(base_size);
  shadowPaint.setImageFilter(SkImageFilters::Blur(
      params.shadow_blur_radius, params.shadow_blur_radius, nullptr));
  canvas.save();
  canvas.translate(0, params.shadow_y_offset);
  canvas.drawPath(icon_grid_bounding_box_path, shadowPaint);
  canvas.restore();

  // Clip to the mask
  canvas.clipPath(icon_grid_bounding_box_path, /*doAntiAlias=*/true);

  // Draw the base icon on a white background
  // If the base icon is opaque, we shouldn't see any white. Unfortunately,
  // first filling the clip with white and then overlaying the base icon
  // results in white artifacts around the corners.  So, we'll use an unclipped
  // intermediate canvas to overlay the base icon on a full white background,
  // and then draw the intermediate canvas in the clip in one shot to avoid
  // white around the edges.
  SkBitmap opaque_bitmap;
  opaque_bitmap.allocPixels(info);
  SkCanvas opaque_canvas(opaque_bitmap);
  SkPaint backgroundPaint;
  backgroundPaint.setStyle(SkPaint::kFill_Style);
  backgroundPaint.setARGB(255, 255, 255, 255);
  opaque_canvas.drawRect(base_rect, backgroundPaint);
  opaque_canvas.drawImage(SkImages::RasterFromBitmap(base_icon.AsBitmap()), 0,
                          0);
  canvas.drawImage(SkImages::RasterFromBitmap(opaque_bitmap), 0, 0);

  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace

namespace web_app {

namespace testing {
namespace {
bool g_disable_icon_masking_for_testing = false;
}  // namespace

base::AutoReset<bool> SetDisableIconMaskingForTesting(bool disabled) {
  return base::AutoReset<bool>(&g_disable_icon_masking_for_testing, disabled);
}
}  // namespace testing

gfx::Image CreateAppleMaskedAppIcon(const gfx::Image& base_icon) {
  int base_size = base_icon.Width();
  IconGridParameters params = GetIconGridParameters(base_size);
  SkPath icon_grid_bounding_box_path =
      CreateMaskingIconGridBoundingBoxPath(base_size, params);

  return CreateAppleMaskedAppIconWithPath(base_icon,
                                          icon_grid_bounding_box_path);
}

NSImageRep* OverlayImageRep(NSImage* background, NSImageRep* overlay) {
  DCHECK(background);
  NSInteger dimension = overlay.pixelsWide;
  DCHECK_EQ(dimension, overlay.pixelsHigh);
  NSBitmapImageRep* canvas = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nullptr
                    pixelsWide:dimension
                    pixelsHigh:dimension
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0];

  // There isn't a colorspace name constant for sRGB, so retag.
  canvas = [canvas
      bitmapImageRepByRetaggingWithColorSpace:NSColorSpace.sRGBColorSpace];

  // Communicate the DIP scale (1.0). TODO(tapted): Investigate HiDPI.
  canvas.size = NSMakeSize(dimension, dimension);

  NSGraphicsContext* drawing_context =
      [NSGraphicsContext graphicsContextWithBitmapImageRep:canvas];
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext.currentContext = drawing_context;
  [background drawInRect:NSMakeRect(0, 0, dimension, dimension)
                fromRect:NSZeroRect
               operation:NSCompositingOperationCopy
                fraction:1.0];
  [overlay drawInRect:NSMakeRect(0, 0, dimension, dimension)
             fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver
             fraction:1.0
       respectFlipped:NO
                hints:nil];
  [NSGraphicsContext restoreGraphicsState];
  return canvas;
}

gfx::Image MaskDiyAppIcon(const gfx::Image& icon) {
  if (testing::g_disable_icon_masking_for_testing) {
    return icon;
  }

  // If the alpha value of the color is less than this value then it is ignored
  constexpr uint8_t kMinAlpha = 1;
  // Maximum allowed deviation between RGB components of colors outside the mask
  constexpr int kMaxNonAlphaDeviation = 5;

  IconGridParameters grid_params = GetIconGridParameters(icon.Width());
  SkPath mask = CreateMaskingIconGridBoundingBoxPath(icon.Width(), grid_params);

  if (AreColorsOutsideMaskWithinDeviation(*icon.ToSkBitmap(), mask, kMinAlpha,
                                          kMaxNonAlphaDeviation)) {
    return CreateAppleMaskedAppIconWithPath(icon, mask);
  } else {
    gfx::Image scaled_icon = ScaleDownInsideMask(icon, grid_params);
    return CreateAppleMaskedAppIconWithPath(scaled_icon, mask);
  }
}

}  // namespace web_app
