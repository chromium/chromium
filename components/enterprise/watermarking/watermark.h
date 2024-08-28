// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_
#define COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class RenderText;
class Rect;
class FontList;
}  // namespace gfx

namespace enterprise_watermark {

// Creates a RenderText instance with a fill style.
std::unique_ptr<gfx::RenderText> CreateFillRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text);

// Creates a RenderTextInstance with a stroke style for text outlines.
std::unique_ptr<gfx::RenderText> CreateOutlineRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text);

// Draws a watermark on the surface represented by the gfx::Canvas instance.
// In this direct refactor, text_fill and text_outline should have the same
// state with the exception of the fill style.
void DrawWatermark(gfx::Canvas* canvas,
                   gfx::RenderText* text_fill,
                   gfx::RenderText* text_outline,
                   int block_height,
                   SkColor background_color,
                   const gfx::Rect& contents_bounds,
                   int block_width);

// Returns the default, hard-coded font list for Chrome watermarks.
const gfx::FontList& WatermarkFontList();

}  // namespace enterprise_watermark

#endif  // COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_
