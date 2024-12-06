// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/watermark.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skia_paint_canvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/render_text.h"

namespace {

// UX Requirements:
constexpr int kWatermarkBlockSpacing = 80;
constexpr int kWatermarkBlockWidth = 350;
constexpr double kRotationAngle = 45;
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

const gfx::FontList& WatermarkFontList() {
  static base::NoDestructor<gfx::FontList> font_list(WatermarkFont());
  return *font_list;
}

gfx::Font::Weight WatermarkFontWeight() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  return gfx::Font::Weight::SEMIBOLD;
#else
  return gfx::Font::Weight::MEDIUM;
#endif
}

std::unique_ptr<gfx::RenderText> CreateRenderText(const gfx::Rect& display_rect,
                                                  const std::u16string& text,
                                                  const SkColor color) {
  auto render_text = gfx::RenderText::CreateRenderText();
  render_text->set_clip_to_display_rect(false);
  render_text->SetFontList(WatermarkFontList());
  render_text->SetWeight(WatermarkFontWeight());
  render_text->SetDisplayOffset(gfx::Vector2d(0, 0));
  render_text->SetDisplayRect(display_rect);
  render_text->SetText(text);
  render_text->SetMultiline(true);
  render_text->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
  render_text->SetColor(color);
  return render_text;
}

int block_width_offset(int block_width) {
  return block_width + kWatermarkBlockSpacing;
}

int block_height_offset(int block_height) {
  return block_height + kWatermarkBlockSpacing;
}

int min_x(double angle, const SkSize& bounds, int block_width) {
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

int max_x(double angle, const SkSize& bounds, int block_width) {
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

int min_y(double angle, const SkSize& bounds) {
  // Instead of starting at Y=0, starting at `kTextSize` lets the first line of
  // text be in frame as text is drawn with (0,0) as the bottom-left corner.
  return kTextSize;
}

int max_y(double angle, const SkSize& bounds) {
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

class WatermarkBlockRenderer {
 public:
  WatermarkBlockRenderer() = default;
  virtual ~WatermarkBlockRenderer();

  virtual void DrawTextBlock(SkScalar x, SkScalar y) = 0;
  virtual void RotateCanvas(SkScalar angle) = 0;
  virtual void Save() = 0;
  virtual void Restore() = 0;
};

WatermarkBlockRenderer::~WatermarkBlockRenderer() = default;

class SkiaWatermarkBlockRenderer : public WatermarkBlockRenderer {
 public:
  SkiaWatermarkBlockRenderer(SkCanvas* canvas, SkPicture* picture)
      : canvas_(canvas), picture_(picture) {}

  void DrawTextBlock(SkScalar x, SkScalar y) override {
    canvas_->save();
    canvas_->translate(x, y);
    picture_->playback(canvas_);
    canvas_->restore();
  }

  void RotateCanvas(SkScalar angle) override { canvas_->rotate(angle); }

  void Save() override { canvas_->save(); }

  void Restore() override { canvas_->restore(); }

 private:
  raw_ptr<SkCanvas> canvas_;
  raw_ptr<SkPicture> picture_;
};

class PaintCanvasWatermarkBlockRenderer : public WatermarkBlockRenderer {
 public:
  PaintCanvasWatermarkBlockRenderer(cc::PaintCanvas* canvas,
                                    cc::PaintRecord* record)
      : canvas_(canvas), record_(record) {}

  void DrawTextBlock(SkScalar x, SkScalar y) override {
    canvas_->save();
    canvas_->translate(x, y);
    canvas_->drawPicture(*record_);
    canvas_->restore();
  }

  void RotateCanvas(SkScalar angle) override { canvas_->rotate(angle); }

  void Save() override { canvas_->save(); }

  void Restore() override { canvas_->restore(); }

 private:
  raw_ptr<cc::PaintCanvas> canvas_;
  raw_ptr<cc::PaintRecord> record_;
};

void DrawWatermark(WatermarkBlockRenderer* watermark_block_renderer,
                   int block_width,
                   int block_height,
                   const SkSize& contents_bounds) {
  watermark_block_renderer->Save();
  watermark_block_renderer->RotateCanvas(360 - kRotationAngle);

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

      watermark_block_renderer->DrawTextBlock(x - stagger, y);
    }
  }
  watermark_block_renderer->Restore();
}

}  // namespace

namespace enterprise_watermark {

int GetWatermarkBlockHeight(const std::u16string& utf16_text,
                            int line_count,
                            int block_width,
                            int text_size) {
  int line_height = 0;
  gfx::Canvas::SizeStringInt(utf16_text, WatermarkFontList(), &block_width,
                             &line_height, text_size, gfx::Canvas::NO_ELLIPSIS);
  return line_height * line_count;
}

std::unique_ptr<gfx::RenderText> CreateFillRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text,
    const SkColor color) {
  auto render_text = CreateRenderText(display_rect, text, color);
  render_text->SetFillStyle(cc::PaintFlags::kFill_Style);
  return render_text;
}

std::unique_ptr<gfx::RenderText> CreateOutlineRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text,
    const SkColor color) {
  auto render_text = CreateRenderText(display_rect, text, color);
  render_text->SetFillStyle(cc::PaintFlags::kStroke_Style);
  return render_text;
}

void DrawWatermark(cc::PaintCanvas* canvas,
                   cc::PaintRecord* record,
                   int block_width,
                   int block_height,
                   const SkSize& contents_bounds) {
  PaintCanvasWatermarkBlockRenderer renderer(canvas, record);
  DrawWatermark(&renderer, block_width, block_height, contents_bounds);
}

void DrawWatermark(SkCanvas* canvas,
                   SkPicture* picture,
                   int block_width,
                   int block_height,
                   const SkSize& contents_bounds) {
  SkiaWatermarkBlockRenderer renderer(canvas, picture);
  DrawWatermark(&renderer, block_width, block_height, contents_bounds);
}

WatermarkBlock DrawWatermarkToPaintRecord(const std::string& watermark_text,
                                          SkColor fill_color,
                                          SkColor outline_color) {
  std::u16string utf16_text = base::UTF8ToUTF16(watermark_text);

  WatermarkBlock watermark_block;
  watermark_block.width = kWatermarkBlockWidth;

  // The coordinates here do not matter as the display rect will change for
  // each drawn block.
  cc::PaintRecorder recorder;
  cc::PaintCanvas* paint_canvas = recorder.beginRecording();
  if (!watermark_text.empty()) {
    gfx::Rect display_rect(0, 0, watermark_block.width, 0);
    auto text_fill = CreateFillRenderText(display_rect, utf16_text, fill_color);
    auto text_outline =
        CreateOutlineRenderText(display_rect, utf16_text, outline_color);
    gfx::Canvas gfx_canvas(paint_canvas, 1.0f);
    text_fill->Draw(&gfx_canvas);
    text_outline->Draw(&gfx_canvas);
    watermark_block.height = GetWatermarkBlockHeight(
        utf16_text, text_fill->GetNumLines(), watermark_block.width, kTextSize);
  } else {
    watermark_block.height = 0;
  }

  watermark_block.record = recorder.finishRecordingAsPicture();
  return watermark_block;
}

void DrawWatermark(SkCanvas* canvas,
                   SkSize size,
                   const std::string& text,
                   int block_width,
                   int text_size) {
  WatermarkBlock block =
      DrawWatermarkToPaintRecord(text, SkColorSetARGB(0xb, 0x00, 0x00, 0x00),
                                 SkColorSetARGB(0x11, 0xff, 0xff, 0xff));
  cc::SkiaPaintCanvas skp_canvas(canvas);
  DrawWatermark(&skp_canvas, &block.record, block.width, block.height, size);
}

}  // namespace enterprise_watermark
