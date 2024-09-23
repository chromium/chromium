// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_shortcut_image.h"

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

AppShortcutImage::AppShortcutImage(int main_icon_radius,
                                   int teardrop_border_radius,
                                   int badge_radius,
                                   SkColor color,
                                   const gfx::ImageSkia& main_icon_image,
                                   const gfx::ImageSkia& badge_image)
    : gfx::CanvasImageSource(
          gfx::Size(main_icon_radius * 2, main_icon_radius * 2)),
      main_icon_radius_(main_icon_radius),
      teardrop_corner_radius_(teardrop_border_radius),
      badge_radius_(badge_radius),
      color_(color),
      main_icon_image_(main_icon_image),
      badge_image_(badge_image) {}

void AppShortcutImage::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color_);

  // Draw the icon background
  const SkScalar kRadius[8] = {SkIntToScalar(main_icon_radius_),
                               SkIntToScalar(main_icon_radius_),
                               SkIntToScalar(main_icon_radius_),
                               SkIntToScalar(main_icon_radius_),
                               SkIntToScalar(teardrop_corner_radius_),
                               SkIntToScalar(teardrop_corner_radius_),
                               SkIntToScalar(main_icon_radius_),
                               SkIntToScalar(main_icon_radius_)};
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(gfx::Size(
                        2 * main_icon_radius_, 2 * main_icon_radius_))),
                    kRadius);
  canvas->DrawPath(path, flags);

  // Draw the icon image
  const int icon_x = main_icon_radius_ - main_icon_image_.width() / 2;
  const int icon_y = main_icon_radius_ - main_icon_image_.height() / 2;
  canvas->DrawImageInt(main_icon_image_, icon_x, icon_y);

  // Draw the badge background
  gfx::Point badge_midpoint(main_icon_radius_ + (base::i18n::IsRTL() ? -1 : 1) *
                                                    main_icon_radius_ / 2,
                            main_icon_radius_ + main_icon_radius_ / 2);
  canvas->DrawCircle(badge_midpoint, badge_radius_, flags);

  // Draw the badge image
  gfx::Point badge_image_coordinate =
      badge_midpoint -
      gfx::Vector2d(badge_image_.width() / 2, badge_image_.height() / 2);
  canvas->DrawImageInt(badge_image_, badge_image_coordinate.x(),
                       badge_image_coordinate.y());
}

// static
gfx::ImageSkia AppShortcutImage::CreateImageWithBadgeAndTeardropBackground(
    int main_icon_radius,
    int teardrop_corner_radius,
    int badge_radius,
    SkColor color,
    const gfx::ImageSkia& main_icon_image,
    const gfx::ImageSkia& badge_image) {
  CHECK_GE(main_icon_radius * 2, main_icon_image.width());
  CHECK_GE(main_icon_radius * 2, main_icon_image.height());
  return gfx::CanvasImageSource::MakeImageSkia<AppShortcutImage>(
      main_icon_radius, teardrop_corner_radius, badge_radius, color,
      main_icon_image, badge_image);
}

}  // namespace apps
