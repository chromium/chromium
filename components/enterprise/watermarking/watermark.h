// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_
#define COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"

class SkCanvas;
struct SkSize;

namespace gfx {
class Canvas;
class RenderText;
class Rect;
class FontList;
}  // namespace gfx

namespace enterprise_watermark {

struct WatermarkStyle {
  int block_width;
  int text_size;
  SkColor fill_color;
  SkColor outline_color;
};

// Utility function to get height of a watermark block. The block height is
// going to be the max required height for a single line times the number of
// line.
int GetWatermarkBlockHeight(const std::u16string& utf16_text,
                            int line_count,
                            int block_width,
                            int text_size);

// Creates a RenderText instance with a fill style.
std::unique_ptr<gfx::RenderText> CreateFillRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text,
    const SkColor color);

// Creates a RenderTextInstance with a stroke style for text outlines.
std::unique_ptr<gfx::RenderText> CreateOutlineRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text,
    const SkColor color);

// Draws a watermark on the surface represented by the gfx::Canvas instance.
// In this direct refactor, text_fill and text_outline should have the same
// state with the exception of the fill style.
void DrawWatermark(gfx::Canvas* canvas,
                   gfx::RenderText* text_fill,
                   gfx::RenderText* text_outline,
                   int block_height,
                   const gfx::Rect& contents_bounds,
                   int block_width);

// Convenience function that creates the required RenderText instances and
// computes the required block_height based on inputs. This overload is useful
// for the case of print where those values are not cached as they are in
// WatermarkView.
void DrawWatermark(SkCanvas* canvas,
                   SkSize size,
                   const std::string& text,
                   int block_width,
                   int text_size);

// Returns the default, hard-coded font list for Chrome watermarks.
const gfx::FontList& WatermarkFontList();

}  // namespace enterprise_watermark

#endif  // COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_
