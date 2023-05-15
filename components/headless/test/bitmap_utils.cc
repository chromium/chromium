// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/bitmap_utils.h"

#include "base/logging.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

bool CheckColoredRect(const SkBitmap& bitmap,
                      SkColor rect_color,
                      SkColor bkgr_color,
                      int margins) {
  gfx::Rect body(bitmap.width(), bitmap.height());
  if (margins) {
    body.Inset(margins);
  }

  // Build color rectangle by including every pixel with the specified
  // rectangle color into a rectangle.
  gfx::Rect rect;
  for (int y = body.y(); y < body.bottom(); y++) {
    for (int x = body.x(); x < body.right(); x++) {
      SkColor color = bitmap.getColor(x, y);
      if (color == rect_color) {
        gfx::Rect pixel_rect(x, y, 1, 1);
        if (rect.IsEmpty()) {
          rect = pixel_rect;
        } else {
          rect.Union(pixel_rect);
        }
      }
    }
  }

  if (rect.IsEmpty()) {
    LOG(ERROR) << "Empty color rectangle, body=" << body.ToString();
    return false;
  }

  // Calculate rectangle to accommodate for pixel anti aliasing.
  gfx::Rect anti_aliasing_rect = rect;
  anti_aliasing_rect.Outset(1);

  // Verify that all pixels outside the found color rectangle are of
  // the specified background color, and the ones that are inside
  // the found rectangle are all of the rectangle color.
  for (int y = body.y(); y < body.bottom(); y++) {
    for (int x = body.x(); x < body.right(); x++) {
      SkColor color = bitmap.getColor(x, y);
      gfx::Point pt(x, y);
      if (rect.Contains(pt)) {
        if (color != rect_color) {
          LOG(ERROR) << "pt=" << pt.ToString() << " color=" << std::hex << color
                     << ", expected rect color=" << std::hex << rect_color
                     << ", color rect=" << rect.ToString();
          return false;
        }
      } else {
        // Expect background color unless the pixel is in anti aliasing
        // rectangle which is adjacent to the actual color rectangle.
        if (color != bkgr_color && !anti_aliasing_rect.Contains(pt)) {
          LOG(ERROR) << "pt=" << pt.ToString() << " color=" << std::hex << color
                     << ", expected bkgr color=" << std::hex << bkgr_color
                     << ", color rect=" << rect.ToString();
          return false;
        }
      }
    }
  }

  return true;
}

bool CheckColoredRect(const SkBitmap& bitmap,
                      SkColor rect_color,
                      SkColor bkgr_color) {
  return CheckColoredRect(bitmap, rect_color, bkgr_color, /*margins=*/0);
}

}  // namespace headless
