// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_icon_generator.h"

#include <cctype>
#include <string>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"

namespace web_app {

namespace {

// Generates a square container icon of |output_size| by drawing the given
// |icon_letter| into a rounded background of |color|.
class GeneratedIconImageSource : public gfx::CanvasImageSource {
 public:
  explicit GeneratedIconImageSource(char16_t icon_letter,
                                    SkColor color,
                                    SquareSizePx output_size)
      : gfx::CanvasImageSource(gfx::Size(output_size, output_size)),
        icon_letter_(icon_letter),
        color_(color),
        output_size_(output_size) {}
  GeneratedIconImageSource(const GeneratedIconImageSource&) = delete;
  GeneratedIconImageSource& operator=(const GeneratedIconImageSource&) = delete;
  ~GeneratedIconImageSource() override = default;

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    SquareSizePx icon_size = output_size_ * 3 / 4;
    int icon_inset = output_size_ / 8;
    const size_t border_radius = output_size_ / 16;
    const size_t font_size = output_size_ * 7 / 16;

    std::string font_name =
        l10n_util::GetStringUTF8(IDS_SANS_SERIF_FONT_FAMILY);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
      // With adaptive icons, we generate full size square icons as they will be
      // masked by the OS.
      icon_size = output_size_;
      icon_inset = 0;
    }
    const std::string kChromeOSFontFamily = "Noto Sans";
    font_name = kChromeOSFontFamily;
#endif

    // Draw a rounded rect of the given |color|.
    cc::PaintFlags background_flags;
    background_flags.setAntiAlias(true);
    background_flags.setColor(color_);

    gfx::Rect icon_rect(icon_inset, icon_inset, icon_size, icon_size);
    canvas->DrawRoundRect(icon_rect, border_radius, background_flags);

    // The text rect's size needs to be odd to center the text correctly.
    gfx::Rect text_rect(icon_inset, icon_inset, icon_size + 1, icon_size + 1);
    canvas->DrawStringRectWithFlags(
        std::u16string(1, icon_letter_),
        gfx::FontList(gfx::Font(font_name, font_size)),
        color_utils::GetColorWithMaxContrast(color_), text_rect,
        gfx::Canvas::TEXT_ALIGN_CENTER);
  }

  char16_t icon_letter_;

  SkColor color_;

  int output_size_;

};

// Adds a square container icon of |output_size| and 2 * |output_size| pixels
// to |bitmaps| by drawing the given |icon_letter| into a rounded background of
// |color|. For each size, if an icon of the requested size already exists in
// |bitmaps|, nothing will happen.
void GenerateIcon(SizeToBitmap* bitmaps,
                  SquareSizePx output_size,
                  SkColor color,
                  char16_t icon_letter) {
  // Do nothing if there is already an icon of |output_size|.
  if (bitmaps->count(output_size))
    return;

  (*bitmaps)[output_size] = GenerateBitmap(output_size, color, icon_letter);
}

void GenerateIcons(std::set<SquareSizePx> generate_sizes,
                   char16_t icon_letter,
                   SkColor generated_icon_color,
                   SizeToBitmap* bitmap_map) {
  // If no color has been specified, use a dark gray so it will stand out on the
  // black shelf.
  if (generated_icon_color == SK_ColorTRANSPARENT)
    generated_icon_color = SK_ColorDKGRAY;

  for (SquareSizePx size : generate_sizes)
    GenerateIcon(bitmap_map, size, generated_icon_color, icon_letter);
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

SkBitmap GenerateBitmap(SquareSizePx output_size,
                        SkColor color,
                        char16_t icon_letter) {
  gfx::ImageSkia icon_image(std::make_unique<GeneratedIconImageSource>(
                                icon_letter, color, output_size),
                            gfx::Size(output_size, output_size));
  SkBitmap dst;
  if (dst.tryAllocPixels(icon_image.bitmap()->info())) {
    icon_image.bitmap()->readPixels(dst.info(), dst.getPixels(), dst.rowBytes(),
                                    0, 0);
  }
  return dst;
}

char16_t GenerateIconLetterFromUrl(const GURL& app_url) {
  std::string app_url_part = " ";
  const std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          app_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (!domain_and_registry.empty()) {
    app_url_part = domain_and_registry;
  } else if (app_url.has_host()) {
    app_url_part = app_url.host();
  }

  // Translate punycode into unicode before retrieving the first letter.
  const std::u16string string_for_display =
      url_formatter::IDNToUnicode(app_url_part);

  char16_t icon_letter = base::i18n::ToUpper(string_for_display)[0];
  return icon_letter;
}

char16_t GenerateIconLetterFromAppName(const std::u16string& app_name) {
  CHECK(!app_name.empty());
  return base::i18n::ToUpper(app_name)[0];
}

SizeToBitmap ResizeIconsAndGenerateMissing(
    const std::vector<SkBitmap>& icons,
    const std::set<SquareSizePx>& sizes_to_generate,
    char16_t icon_letter,
    SkColor* generated_icon_color,
    bool* is_generated_icon) {
  DCHECK(generated_icon_color);
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
    // Determine the color that will be used for the icon's background. For this
    // the dominant color of the first icon found is used.
    color_utils::GridSampler sampler;
    *generated_icon_color = color_utils::CalculateKMeanColorOfBitmap(
        resized_bitmaps.begin()->second);

    *is_generated_icon = false;
    // ConstrainBitmapsToSizes generates versions for each size in
    // |sizes_to_generate|, so we don't need to generate icons.
    return resized_bitmaps;
  }

  *is_generated_icon = true;
  GenerateIcons(sizes_to_generate, icon_letter, *generated_icon_color,
                &resized_bitmaps);

  return resized_bitmaps;
}

SizeToBitmap GenerateIcons(const std::string& app_name,
                           SkColor background_icon_color) {
  const std::u16string app_name_utf16 = base::UTF8ToUTF16(app_name);
  const char16_t icon_letter = GenerateIconLetterFromAppName(app_name_utf16);

  SizeToBitmap icons;
  for (SquareSizePx size : SizesToGenerate()) {
    icons[size] = GenerateBitmap(size, background_icon_color, icon_letter);
  }
  return icons;
}

}  // namespace web_app
