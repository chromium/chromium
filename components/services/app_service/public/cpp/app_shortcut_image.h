// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SHORTCUT_IMAGE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SHORTCUT_IMAGE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace gfx {
class Canvas;
class ImageSkia;
}  // namespace gfx
   //
namespace apps {

// A badged image with a "teardrop" shaped background.
// The image will have a circular background of radius `main_icon_radius`, with
// a rounded rect bottom right corner with radius `teardrop_corner_radius`.
// `main_icon_image` is painted centered to the main icon background.
// The main image is badged with `badge_image` in bottom right quarter of the
// icon. The badge itself has a circular background with `badge_radius`.
// The icon is intended to be used as an icon for app shortcuts in different UI
// surfaces.
class AppShortcutImage : public gfx::CanvasImageSource {
 public:
  AppShortcutImage(int main_icon_radius,
                   int teardrop_corner_radius,
                   int badge_radius,
                   SkColor color,
                   const gfx::ImageSkia& main_icon_image,
                   const gfx::ImageSkia& badge_image);
  AppShortcutImage(const AppShortcutImage&) = delete;
  AppShortcutImage& operator=(const AppShortcutImage&) = delete;
  ~AppShortcutImage() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

  static gfx::ImageSkia CreateImageWithBadgeAndTeardropBackground(
      int main_icon_radius,
      int teardrop_corner_radius,
      int badge_radius,
      SkColor color,
      const gfx::ImageSkia& main_icon_image,
      const gfx::ImageSkia& badge_image);

 private:
  const int main_icon_radius_;
  const int teardrop_corner_radius_;
  const int badge_radius_;
  const SkColor color_;
  const gfx::ImageSkia main_icon_image_;
  const gfx::ImageSkia badge_image_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SHORTCUT_IMAGE_H_
