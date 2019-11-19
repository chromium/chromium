// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_icon_generator.h"

#include <cctype>
#include <string>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/web_application_info.h"
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
// |letter| into a rounded background of |color|.
class GeneratedIconImageSource : public gfx::CanvasImageSource {
 public:
  explicit GeneratedIconImageSource(base::char16 letter,
                                    SkColor color,
                                    int output_size)
      : gfx::CanvasImageSource(gfx::Size(output_size, output_size)),
        letter_(letter),
        color_(color),
        output_size_(output_size) {}
  ~GeneratedIconImageSource() override {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    const int icon_size = output_size_ * 3 / 4;
    const int icon_inset = output_size_ / 8;
    const size_t border_radius = output_size_ / 16;
    const size_t font_size = output_size_ * 7 / 16;

    std::string font_name =
        l10n_util::GetStringUTF8(IDS_SANS_SERIF_FONT_FAMILY);
#if defined(OS_CHROMEOS)
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
        base::string16(1, letter_),
        gfx::FontList(gfx::Font(font_name, font_size)),
        color_utils::GetColorWithMaxContrast(color_), text_rect,
        gfx::Canvas::TEXT_ALIGN_CENTER);
  }

  base::char16 letter_;

  SkColor color_;

  int output_size_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedIconImageSource);
};

// Adds a square container icon of |output_size| and 2 * |output_size| pixels
// to |bitmaps| by drawing the given |letter| into a rounded background of
// |color|. For each size, if an icon of the requested size already exists in
// |bitmaps|, nothing will happen.
void GenerateIcon(std::map<int, BitmapAndSource>* bitmaps,
                  int output_size,
                  SkColor color,
                  base::char16 letter) {
  // Do nothing if there is already an icon of |output_size|.
  if (bitmaps->count(output_size))
    return;

  (*bitmaps)[output_size].bitmap = GenerateBitmap(output_size, color, letter);
}

void GenerateIcons(std::set<int> generate_sizes,
                   const GURL& app_url,
                   SkColor generated_icon_color,
                   std::map<int, BitmapAndSource>* bitmap_map) {
  // If no color has been specified, use a dark gray so it will stand out on the
  // black shelf.
  if (generated_icon_color == SK_ColorTRANSPARENT)
    generated_icon_color = SK_ColorDKGRAY;

  const base::char16 icon_letter = GenerateIconLetterFromUrl(app_url);

  for (int size : generate_sizes)
    GenerateIcon(bitmap_map, size, generated_icon_color, icon_letter);
}

}  // namespace

std::set<int> SizesToGenerate() {
  return std::set<int>({
      icon_size::k32,
      icon_size::k64,
      icon_size::k48,
      icon_size::k96,
      icon_size::k128,
      icon_size::k256,
  });
}

BitmapAndSource::BitmapAndSource() {}

BitmapAndSource::BitmapAndSource(const GURL& source_url_p,
                                 const SkBitmap& bitmap_p)
    : source_url(source_url_p), bitmap(bitmap_p) {}

BitmapAndSource::~BitmapAndSource() {}

std::map<int, BitmapAndSource> ConstrainBitmapsToSizes(
    const std::vector<BitmapAndSource>& bitmaps,
    const std::set<int>& sizes) {
  std::map<int, BitmapAndSource> output_bitmaps;
  std::map<int, BitmapAndSource> ordered_bitmaps;
  for (const BitmapAndSource& bitmap_and_source : bitmaps) {
    const SkBitmap& bitmap = bitmap_and_source.bitmap;
    DCHECK(bitmap.width() == bitmap.height());
    ordered_bitmaps[bitmap.width()] = bitmap_and_source;
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
      if (output_bitmaps[size].bitmap.width() != size) {
        output_bitmaps[size].bitmap = skia::ImageOperations::Resize(
            output_bitmaps[size].bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
            size, size);
      }
    }
  }

  return output_bitmaps;
}

SkBitmap GenerateBitmap(int output_size, SkColor color, base::char16 letter) {
  gfx::ImageSkia icon_image(
      std::make_unique<GeneratedIconImageSource>(letter, color, output_size),
      gfx::Size(output_size, output_size));
  SkBitmap dst;
  if (dst.tryAllocPixels(icon_image.bitmap()->info())) {
    icon_image.bitmap()->readPixels(dst.info(), dst.getPixels(), dst.rowBytes(),
                                    0, 0);
  }
  return dst;
}

// Returns the letter that will be painted on the generated icon.
base::char16 GenerateIconLetterFromUrl(const GURL& app_url) {
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
  const base::string16 string_for_display =
      url_formatter::IDNToUnicode(app_url_part);

  base::char16 icon_letter = base::i18n::ToUpper(string_for_display)[0];
  return icon_letter;
}

std::map<int, BitmapAndSource> ResizeIconsAndGenerateMissing(
    const std::vector<BitmapAndSource>& icons,
    const std::set<int>& sizes_to_generate,
    const GURL& app_url,
    SkColor* generated_icon_color) {
  DCHECK(generated_icon_color);

  // Resize provided icons to make sure we have versions for each size in
  // |sizes_to_generate|.
  std::map<int, BitmapAndSource> resized_bitmaps(
      ConstrainBitmapsToSizes(icons, sizes_to_generate));

  // Also add all provided icon sizes.
  for (const BitmapAndSource& icon : icons) {
    if (resized_bitmaps.find(icon.bitmap.width()) == resized_bitmaps.end())
      resized_bitmaps.insert(std::make_pair(icon.bitmap.width(), icon));
  }

  // Determine the color that will be used for the icon's background. For this
  // the dominant color of the first icon found is used.
  if (!resized_bitmaps.empty()) {
    color_utils::GridSampler sampler;
    *generated_icon_color = color_utils::CalculateKMeanColorOfBitmap(
        resized_bitmaps.begin()->second.bitmap);
  }

  // Work out what icons we need to generate here. Icons are only generated if
  // there is no icon in the required size.
  std::set<int> generate_sizes;
  for (int size : sizes_to_generate) {
    if (resized_bitmaps.find(size) == resized_bitmaps.end())
      generate_sizes.insert(size);
  }
  GenerateIcons(generate_sizes, app_url, *generated_icon_color,
                &resized_bitmaps);

  return resized_bitmaps;
}

std::vector<WebApplicationIconInfo> GenerateIcons(
    const std::string& app_name,
    SkColor background_icon_color) {
  const std::set<int> sizes_to_generate = SizesToGenerate();

  std::vector<WebApplicationIconInfo> icon_infos;
  icon_infos.reserve(sizes_to_generate.size());

  const base::string16 app_name_utf16 = base::UTF8ToUTF16(app_name);
  CHECK(!app_name_utf16.empty());
  const base::char16 first_app_name_letter =
      base::i18n::ToUpper(app_name_utf16)[0];

  for (int size : sizes_to_generate) {
    SkBitmap bitmap =
        GenerateBitmap(size, background_icon_color, first_app_name_letter);

    WebApplicationIconInfo icon_info;
    icon_info.data = bitmap;
    icon_info.width = bitmap.width();
    icon_info.height = bitmap.height();
    // Leave icon_info.url empty to indicate that this is generated icon.
    icon_infos.push_back(std::move(icon_info));
  }

  return icon_infos;
}

}  // namespace web_app
