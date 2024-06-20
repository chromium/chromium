// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/check_op.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/gfx/image/image.h"

namespace web_app {

gfx::Image CreateAppleMaskedAppIcon(const gfx::Image& base_icon) {
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
  int base_size = base_icon.Width();
  SkScalar icon_grid_bounding_box_inset;
  SkScalar icon_grid_bounding_box_corner_radius;
  SkScalar shadow_y_offset;
  SkScalar shadow_blur_radius;
  switch (base_size) {
    case 16:
      // An exact value for the 16 corner radius was not available.
      // this corner radius is derived by dividing the 32 radius by 2
      icon_grid_bounding_box_inset = 1.0;
      icon_grid_bounding_box_corner_radius = 2.785;
      shadow_y_offset = 0.5;
      shadow_blur_radius = 0.5;
      break;
    case 32:
      icon_grid_bounding_box_inset = 2.0;
      icon_grid_bounding_box_corner_radius = 5.75;
      shadow_y_offset = 1.0;
      shadow_blur_radius = 1.0;
      break;
    case 64:
      icon_grid_bounding_box_inset = 6.0;
      icon_grid_bounding_box_corner_radius = 11.5;
      shadow_y_offset = 2;
      shadow_blur_radius = 2;
      break;
    case 128:
      // An exact value for the 128 corner radius was not available.
      // this corner radius is derived by dividing the 256 radius by 2
      // or by multiplying the 64 radius by 2, both calculations
      // have the same result.
      icon_grid_bounding_box_inset = 12.0;
      icon_grid_bounding_box_corner_radius = 23.0;
      shadow_y_offset = 1.25;
      shadow_blur_radius = 1.25;
      break;
    case 256:
      icon_grid_bounding_box_inset = 25.0;
      icon_grid_bounding_box_corner_radius = 46.0;
      shadow_y_offset = 2.5;
      shadow_blur_radius = 2.5;
      break;
    case 512:
      icon_grid_bounding_box_inset = 50.0;
      icon_grid_bounding_box_corner_radius = 92.0;
      shadow_y_offset = 5.0;
      shadow_blur_radius = 5.0;
      break;
    case 1024:
      // An exact value for the 1024 corner radius was not available.
      // this corner radius is derived by multiplying the 512 radius by 2
      icon_grid_bounding_box_inset = 100.0;
      icon_grid_bounding_box_corner_radius = 184.0;
      shadow_y_offset = 10.0;
      shadow_blur_radius = 10.0;
      break;
    default:
      // Use 1024 sizes as a reference for approximating any size mask if needed
      icon_grid_bounding_box_inset = base_size * 100.0 / 1024.0;
      icon_grid_bounding_box_corner_radius = base_size * 184.0 / 1024.0;
      shadow_y_offset = base_size * 10.0 / 1024.0;
      shadow_blur_radius = base_size * 10.0 / 1024.0;
      break;
  }

  // Creat a bitmap and canvas for drawing the masked icon
  SkImageInfo info =
      SkImageInfo::Make(base_size, base_size, SkColorType::kN32_SkColorType,
                        SkAlphaType::kUnpremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  SkCanvas canvas(bitmap);
  SkRect base_rect = SkRect::MakeIWH(base_size, base_size);

  // Create a path for the icon mask. The mask will match Apple's icon grid
  // bounding box.
  SkPath icon_grid_bounding_box_path;
  SkRect icon_grid_bounding_box_rect = base_rect.makeInset(
      icon_grid_bounding_box_inset, icon_grid_bounding_box_inset);
  icon_grid_bounding_box_path.addRoundRect(
      icon_grid_bounding_box_rect, icon_grid_bounding_box_corner_radius,
      icon_grid_bounding_box_corner_radius);

  // Draw the shadow
  SkPaint shadowPaint;
  shadowPaint.setStyle(SkPaint::kFill_Style);
  shadowPaint.setARGB(77, 0, 0, 0);
  shadowPaint.setImageFilter(
      SkImageFilters::Blur(shadow_blur_radius, shadow_blur_radius, nullptr));
  canvas.save();
  canvas.translate(0, shadow_y_offset);
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

  // Create the final image.
  return gfx::Image::CreateFrom1xBitmap(bitmap);
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

}  // namespace web_app
