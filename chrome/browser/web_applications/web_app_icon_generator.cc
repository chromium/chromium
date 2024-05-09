// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_generator.h"

#include <string>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace web_app {

namespace {

// Adds a square container icon of |output_size| and 2 * |output_size| pixels
// to |bitmaps| by drawing the given |icon_letter| into a rounded background.
// For each size, if an icon of the requested size already exists in |bitmaps|,
// nothing will happen.
void GenerateIcon(SizeToBitmap* bitmaps,
                  SquareSizePx output_size,
                  char32_t icon_letter) {
  // Do nothing if there is already an icon of |output_size|.
  if (bitmaps->count(output_size))
    return;

  (*bitmaps)[output_size] = shortcuts::GenerateBitmap(output_size, icon_letter);
}

void GenerateIcons(const std::set<SquareSizePx>& generate_sizes,
                   char32_t icon_letter,
                   SizeToBitmap* bitmap_map) {
  for (SquareSizePx size : generate_sizes)
    GenerateIcon(bitmap_map, size, icon_letter);
}

}  // namespace

std::set<SquareSizePx> SizesToGenerate() {
  return std::set<SquareSizePx>({
      icon_size::k32,
      icon_size::k64,
      icon_size::k48,
      icon_size::k96,
      icon_size::k128,
      icon_size::k256,
  });
}

SizeToBitmap ConstrainBitmapsToSizes(const std::vector<SkBitmap>& bitmaps,
                                     const std::set<SquareSizePx>& sizes) {
  SizeToBitmap output_bitmaps;
  SizeToBitmap ordered_bitmaps;
  for (const SkBitmap& bitmap : bitmaps) {
    DCHECK(bitmap.width() == bitmap.height());
    ordered_bitmaps[bitmap.width()] = bitmap;
  }

  if (!ordered_bitmaps.empty()) {
    for (const auto& size : sizes) {
      // Find the closest not-smaller bitmap, or failing that use the largest
      // icon available.
      auto bitmaps_it = ordered_bitmaps.lower_bound(size);
      if (bitmaps_it != ordered_bitmaps.end())
        output_bitmaps[size] = bitmaps_it->second;
      else
        output_bitmaps[size] = ordered_bitmaps.rbegin()->second;

      // Resize the bitmap if it does not exactly match the desired size.
      if (output_bitmaps[size].width() != size) {
        output_bitmaps[size] = skia::ImageOperations::Resize(
            output_bitmaps[size], skia::ImageOperations::RESIZE_LANCZOS3, size,
            size);
      }
    }
  }

  return output_bitmaps;
}

SizeToBitmap ResizeIconsAndGenerateMissing(
    const std::vector<SkBitmap>& icons,
    const std::set<SquareSizePx>& sizes_to_generate,
    char32_t icon_letter,
    bool* is_generated_icon) {
  DCHECK(is_generated_icon);

  // Resize provided icons to make sure we have versions for each size in
  // |sizes_to_generate|.
  SizeToBitmap resized_bitmaps(
      ConstrainBitmapsToSizes(icons, sizes_to_generate));

  // Also add all provided icon sizes.
  for (const SkBitmap& icon : icons) {
    if (resized_bitmaps.find(icon.width()) == resized_bitmaps.end())
      resized_bitmaps.insert(std::make_pair(icon.width(), icon));
  }

  if (!resized_bitmaps.empty()) {
    *is_generated_icon = false;
    // ConstrainBitmapsToSizes generates versions for each size in
    // |sizes_to_generate|, so we don't need to generate icons.
    return resized_bitmaps;
  }

  *is_generated_icon = true;
  GenerateIcons(sizes_to_generate, icon_letter, &resized_bitmaps);

  return resized_bitmaps;
}

SizeToBitmap GenerateIcons(const std::string& app_name) {
  const std::u16string app_name_utf16 = base::UTF8ToUTF16(app_name);
  const char32_t icon_letter =
      shortcuts::GenerateIconLetterFromName(app_name_utf16);

  SizeToBitmap icons;
  for (SquareSizePx size : SizesToGenerate()) {
    icons[size] = shortcuts::GenerateBitmap(size, icon_letter);
  }
  return icons;
}

gfx::ImageSkia ConvertImageToSolidFillMonochrome(SkColor solid_color,
                                                 const gfx::ImageSkia& image) {
  if (image.isNull())
    return image;

  // Create a monochrome image by combining alpha channel of |image| and
  // |solid_color|.
  gfx::ImageSkia solid_fill_image =
      gfx::ImageSkiaOperations::CreateColorMask(image, solid_color);

  // Does the actual conversion from the source image.
  for (const gfx::ImageSkiaRep& src_ui_scalefactor_rep : image.image_reps())
    solid_fill_image.GetRepresentation(src_ui_scalefactor_rep.scale());

  // Deletes the pointer to the source image.
  solid_fill_image.MakeThreadSafe();

  return solid_fill_image;
}

}  // namespace web_app
