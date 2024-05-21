// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/enterprise/watermarking/watermark.h"

#include "base/no_destructor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/render_text.h"

namespace {

// UX Requirements:
constexpr int kWatermarkBlockSpacing = 80;
constexpr double kRotationAngle = 45;
constexpr SkColor kFillColor = SkColorSetARGB(0x12, 0x00, 0x00, 0x00);
constexpr SkColor kOutlineColor = SkColorSetARGB(0x27, 0xff, 0xff, 0xff);
constexpr float kTextSize = 24.0f;

gfx::Font WatermarkFont() {
  return gfx::Font(
#if BUILDFLAG(IS_WIN)
      "Segoe UI",
#elif BUILDFLAG(IS_MAC)
      "SF Pro Text",
#elif BUILDFLAG(IS_LINUX)
      "Ubuntu",
#elif BUILDFLAG(IS_CHROMEOS)
      "Google Sans",
#else
      "sans-serif",
#endif
      kTextSize);
}

gfx::Font::Weight WatermarkFontWeight() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  return gfx::Font::Weight::SEMIBOLD;
#else
  return gfx::Font::Weight::MEDIUM;
#endif
}

std::unique_ptr<gfx::RenderText> CreateRenderText(const gfx::Rect& display_rect,
                                                  const std::u16string& text) {
  auto render_text = gfx::RenderText::CreateRenderText();
  render_text->set_clip_to_display_rect(false);
  render_text->SetFontList(enterprise_watermark::WatermarkFontList());
  render_text->SetWeight(WatermarkFontWeight());
  render_text->SetDisplayOffset(gfx::Vector2d(0, 0));
  render_text->SetDisplayRect(display_rect);
  render_text->SetText(text);
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
  return render_text;
}

int block_width_offset(int block_width) {
  return block_width + kWatermarkBlockSpacing;
}

int block_height_offset(int block_height) {
  return block_height + kWatermarkBlockSpacing;
}

int min_x(double angle, const gfx::Rect& bounds, int block_width) {
  // Due to the rotation of the watermark, X needs to start in the negatives so
  // that the rotated canvas is still large enough to cover `bounds`. This means
  // our initial X needs to be proportional to this triangle side:
  //             |
  //   +---------+
  //   |
  //   |     ╱angle
  //   |    ╱┌────────────────────
  //   V   ╱ │
  //      ╱  │
  //   X ╱   │
  //    ╱    │
  //   ╱     │  `bounds`
  //  ╱90    │
  //  ╲deg.  │
  //   ╲     │
  //    ╲    │
  //     ╲   │
  //      ╲  │
  //       ╲ │
  //        ╲│
  //
  // -X also needs to be a factor of `block_width_offset()` so that there is no
  // sliding of the watermark blocks when `bounds` resize and there's always a
  // text block drawn at X=0.
  int min = cos(90 - angle) * bounds.height();
  return -((min / block_width_offset(block_width)) + 1) *
         block_width_offset(block_width);
}

int max_x(double angle, const gfx::Rect& bounds, int block_width) {
  // Due to the rotation of the watermark, X needs to end further then the
  // `bounds` width. This means our final X needs to be proportional to this
  // triangle side:
  //           |
  //           |
  //           |     ╱╲
  //           |    ╱90╲
  //           V   ╱deg.╲
  //              ╱      ╲
  //           X ╱        ╲
  //            ╱          ╲
  //           ╱            ╲
  //          ╱              ╲
  //         ╱angle           ╲
  //        ┌──────────────────┐
  //        │  `bounds`        │
  //
  // An extra `block_width_offset()` length is added so that the last column for
  // staggered rows doesn't appear on resizes.
  return cos(angle) * bounds.width() + block_width_offset(block_width);
}

int min_y(double angle, const gfx::Rect& bounds) {
  // Instead of starting at Y=0, starting at `kTextSize` lets the first line of
  // text be in frame as text is drawn with (0,0) as the bottom-left corner.
  return kTextSize;
}

int max_y(double angle, const gfx::Rect& bounds) {
  // Due to the rotation of the watermark, Y needs to end further then the
  // `bounds` height. This means our final Y needs to be proportional to these
  // two triangle sides:  +-----------+
  //                      |           |
  //                      |           |
  //                 ╱╲   V           |
  //                ╱90╲              |
  //               ╱deg.╲ Y1          |
  //              ╱      ╲            |
  //             ╱        ╲           |
  //            ╱          ╲          |
  //           ╱            ╲         |
  //          ╱              ╲        |
  //         ╱angle           ╲       |
  //        ┌──────────────────┐      |
  //        │  `bounds`        │╲     |
  //                           │ ╲    |
  //                           │  ╲   V
  //                           │   ╲
  //                           │    ╲ Y2
  //                           │     ╲
  //                           │      ╲
  //                           │    90 ╲
  //                           │   deg.╱
  //                           │      ╱
  //                           │     ╱
  //                           │    ╱
  //                           │   ╱
  //                           │  ╱
  //                           │ ╱
  //                           │╱
  //
  return sin(angle) * bounds.width() + cos(angle) * bounds.height();
}

void DrawTextBlock(gfx::Canvas* canvas,
                   int x,
                   int y,
                   gfx::RenderText* text_fill,
                   gfx::RenderText* text_outline,
                   int block_height,
                   int block_width) {
  gfx::Rect display_rect(x, y, block_width, block_height);

  text_fill->SetDisplayRect(display_rect);
  text_fill->Draw(canvas);

  text_outline->SetDisplayRect(display_rect);
  text_outline->Draw(canvas);
}

}  // namespace

namespace enterprise_watermark {

const gfx::FontList& WatermarkFontList() {
  static base::NoDestructor<gfx::FontList> font_list(WatermarkFont());
  return *font_list;
}

std::unique_ptr<gfx::RenderText> CreateFillRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text) {
  auto render_text = CreateRenderText(display_rect, text);
  render_text->SetFillStyle(cc::PaintFlags::kFill_Style);
  render_text->SetColor(kFillColor);
  return render_text;
}

std::unique_ptr<gfx::RenderText> CreateOutlineRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text) {
  auto render_text = CreateRenderText(display_rect, text);
  render_text->SetFillStyle(cc::PaintFlags::kStroke_Style);
  render_text->SetColor(kOutlineColor);
  return render_text;
}

void DrawWatermark(gfx::Canvas* canvas,
                   gfx::RenderText* text_fill,
                   gfx::RenderText* text_outline,
                   int block_height,
                   SkColor background_color,
                   const gfx::Rect& contents_bounds,
                   const gfx::Rect& local_bounds,
                   int block_width) {
  if (!text_fill) {
    DCHECK(!text_outline);
    return;
  }

  canvas->sk_canvas()->rotate(360 - kRotationAngle);

  int upper_x = max_x(kRotationAngle, contents_bounds, block_width);
  int upper_y = max_y(kRotationAngle, contents_bounds);
  for (int x = min_x(kRotationAngle, contents_bounds, block_width);
       x <= upper_x; x += block_width_offset(block_width)) {
    bool apply_stagger = false;
    for (int y = min_y(kRotationAngle, contents_bounds); y < upper_y;
         y += block_height_offset(block_height)) {
      // Every other row, stagger the text horizontally to give a
      // "brick tiling" effect.
      int stagger = apply_stagger ? block_width_offset(block_width) / 2 : 0;
      apply_stagger = !apply_stagger;

      DrawTextBlock(canvas, x - stagger, y, text_fill, text_outline,
                    block_height, block_width);
    }
  }

  // Draw BG
  cc::PaintFlags bgflags;
  bgflags.setColor(background_color);
  bgflags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawRect(local_bounds, bgflags);
}

}  // namespace enterprise_watermark
