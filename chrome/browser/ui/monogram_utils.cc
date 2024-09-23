// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/monogram_utils.h"

#include "chrome/grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"

namespace monogram {
namespace {

// Draws a circle of a given `size` and `offset` in the `canvas` and fills it
// with `background_color`.
void DrawCircleInCanvas(gfx::Canvas* canvas,
                        int size,
                        int offset,
                        SkColor background_color) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(background_color);
  int corner_radius = size / 2;
  canvas->DrawRoundRect(gfx::Rect(offset, offset, size, size), corner_radius,
                        flags);
}

// Will paint `letter` in the center of specified `canvas` of given `size`.
void DrawFallbackIconLetter(gfx::Canvas* canvas,
                            const std::u16string& monogram,
                            SkColor monogram_color,
                            int size,
                            int offset) {
  if (monogram.empty())
    return;

  const double kDefaultFontSizeRatio = 0.5;
  int font_size = static_cast<int>(size * kDefaultFontSizeRatio);
  if (font_size <= 0)
    return;

  gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL;

#if BUILDFLAG(IS_WIN)
  font_weight = gfx::Font::Weight::SEMIBOLD;
#endif

  // TODO(crbug.com/41395192): Adjust the text color according to the background
  // color.
  canvas->DrawStringRectWithFlags(
      monogram,
      gfx::FontList({l10n_util::GetStringUTF8(IDS_NTP_FONT_FAMILY)},
                    gfx::Font::NORMAL, font_size, font_weight),
      monogram_color, gfx::Rect(offset, offset, size, size),
      gfx::Canvas::TEXT_ALIGN_CENTER);
}

}  // namespace

void DrawMonogramInCanvas(gfx::Canvas* canvas,
                          int canvas_size,
                          int circle_size,
                          const std::u16string& monogram,
                          SkColor monogram_color,
                          SkColor background_color) {
  canvas->DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);

  int offset = (canvas_size - circle_size) / 2;
  DrawCircleInCanvas(canvas, circle_size, offset, background_color);
  DrawFallbackIconLetter(canvas, monogram, monogram_color, circle_size, offset);
}

}  // namespace monogram
