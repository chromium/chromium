// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_
#define COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_

#include <string>

#include "cc/paint/paint_record.h"
#include "third_party/skia/include/core/SkColor.h"

class SkCanvas;
struct SkSize;

namespace cc {
class PaintCanvas;
}

namespace gfx {
class RenderText;
class Rect;
}  // namespace gfx

namespace enterprise_watermark {

// A page watermark consists of a unique pattern/object/text, rendered
// repeatedly over the page. We refer to this object as a "Watermark Block".
struct WatermarkBlock {
  cc::PaintRecord record;
  int width;
  int height;
};

// Utility function to get height of a watermark block. The block height is
// going to be the max required height for a single line times the number of
// line.
int GetWatermarkBlockHeight(const std::u16string& utf16_text,
                            int line_count,
                            int block_width,
                            int font_size);

// Creates a RenderText instance with a fill style.
std::unique_ptr<gfx::RenderText> CreateFillRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text,
    SkColor color,
    int font_size);

// Creates a RenderTextInstance with a stroke style for text outlines.
std::unique_ptr<gfx::RenderText> CreateOutlineRenderText(
    const gfx::Rect& display_rect,
    const std::u16string& text,
    SkColor color,
    int font_size);

// Draws a watermark on the surface represented by the cc::PaintCanvas instance.
// `block_width` and `block_height` are the dimensions of the watermark block
// represented by the cc::PaintRecord. `contents_bounds` represents the
// dimensions of the area over which the watermark is drawn.
void DrawWatermark(cc::PaintCanvas* canvas,
                   cc::PaintRecord* record,
                   int block_width,
                   int block_height,
                   const SkSize& contents_bounds);

// Draws a watermark on the surface represented by the SkCanvas instance.
// `block_width` and `block_height` are the dimensions of the watermark block
// represented by the SkPicture. `contents_bounds` represents the
// dimensions of the area over which the watermark is drawn.
void DrawWatermark(SkCanvas* canvas,
                   SkPicture* picture,
                   int block_width,
                   int block_height,
                   const SkSize& contents_bounds);

// Draws a watermark onto a PaintRecord and stores it along with the block's
// width and height so that it can be rendered in processes other than the
// browser.
WatermarkBlock DrawWatermarkToPaintRecord(const std::string& watermark_text,
                                          SkColor fill_color,
                                          SkColor outline_color,
                                          int font_size);

// Previously: a convenience function that creates the required RenderText
// instances and computes the required block_height based on inputs. This
// overload is useful for the case of print where those values are not cached as
// they are in WatermarkView. Will be removed in favour of one of the other
// overloads above.
void DrawWatermark(SkCanvas* canvas,
                   SkSize size,
                   const std::string& text,
                   int block_width,
                   int font_size);
}  // namespace enterprise_watermark

#endif  // COMPONENTS_ENTERPRISE_WATERMARKING_WATERMARK_H_
