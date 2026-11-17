// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/text_document_content.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/rect.h"

namespace mirroring {

namespace {

// Luma values, in limited (studio) range, matching the REC709 color space used
// by the fake capturer.
constexpr uint8_t kPageLuma = 235;     // White page.
constexpr uint8_t kCanvasLuma = 226;   // Light gray canvas around the page.
constexpr uint8_t kToolbarLuma = 230;  // Toolbar background.
constexpr uint8_t kInkLuma = 20;       // Near-black text.
constexpr uint8_t kNeutralChroma = 128;

// Layout constants, in pixels, approximating Google Docs at 100% zoom on a
// 96 DPI display with 11pt Arial text and 1.15 line spacing.
constexpr int kToolbarHeight = 120;
constexpr int kPageWidth = 816;
constexpr int kPageHeight = 1056;
constexpr int kPageGap = 16;
constexpr int kPageMargin = 96;
constexpr int kLinePitch = 20;
constexpr int kBaselineOffset = 15;
constexpr int kXHeight = 7;
constexpr int kAscender = 11;
constexpr int kDescender = 3;
constexpr int kNumPages = 2;

// A simple luma canvas with anti-aliased, ink-on-paper drawing primitives.
class LumaCanvas {
 public:
  LumaCanvas(std::vector<uint8_t>& pixels, int width, int height)
      : pixels_(pixels), width_(width), height_(height) {
    CHECK_EQ(pixels_->size(), static_cast<size_t>(width_ * height_));
  }

  void Fill(const gfx::Rect& rect, uint8_t value) {
    const gfx::Rect clipped =
        gfx::IntersectRects(rect, gfx::Rect(width_, height_));
    for (int y = clipped.y(); y < clipped.bottom(); ++y) {
      std::fill_n(pixels_->begin() + y * width_ + clipped.x(), clipped.width(),
                  value);
    }
  }

  // Blends ink into the pixel at (x, y) with coverage `ink` in [0, 1].
  void Ink(int x, int y, double ink) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
      return;
    }
    uint8_t& pixel = (*pixels_)[y * width_ + x];
    const double value = pixel - ink * (pixel - kInkLuma);
    pixel = static_cast<uint8_t>(std::clamp(value + 0.5, 0.0, 255.0));
  }

  // Vertical stroke about 1.5 px wide, from `top` to `bottom` (exclusive).
  void Stem(int x, int top, int bottom) {
    for (int y = top; y < bottom; ++y) {
      Ink(x, y, 1.0);
      Ink(x + 1, y, 0.45);
    }
  }

  // Horizontal stroke about 1 px tall.
  void Bar(int left, int right, int y, double ink = 0.9) {
    for (int x = left; x < right; ++x) {
      Ink(x, y, ink);
    }
  }

  // A rounded box, used for bowls ("o", "e", "a", ...).
  void Bowl(int left, int right, int top, int bottom) {
    Bar(left + 1, right - 1, top);
    Bar(left + 1, right - 1, bottom - 1);
    for (int y = top + 1; y < bottom - 1; ++y) {
      Ink(left, y, 1.0);
      Ink(right - 1, y, 1.0);
    }
    // Soft corners.
    Ink(left, top, 0.35);
    Ink(right - 1, top, 0.35);
    Ink(left, bottom - 1, 0.35);
    Ink(right - 1, bottom - 1, 0.35);
  }

 private:
  const raw_ref<std::vector<uint8_t>> pixels_;
  const int width_;
  const int height_;
};

// Seeded Lehmer PRNG (`x_{n+1} = 48271 * x_n mod (2^31 - 1)`), used so layout
// generation is deterministic across platforms without `<random>`.
class LehmerRng {
 public:
  explicit LehmerRng(uint32_t seed)
      : state_(seed % kModulus == 0 ? 1u : seed % kModulus) {}

  // Uniform integer in [lo, hi].
  int RandomInt(int lo, int hi) {
    CHECK_LE(lo, hi);
    state_ = static_cast<uint32_t>((uint64_t{state_} * kMultiplier) % kModulus);
    const uint32_t range = static_cast<uint32_t>(hi - lo + 1);
    return lo + static_cast<int>((state_ - 1u) % range);
  }

 private:
  static constexpr uint32_t kModulus = 2147483647u;
  static constexpr uint64_t kMultiplier = 48271u;
  uint32_t state_;
};

// Draws a single glyph-like shape with its left edge at `x` and baseline at
// `baseline`. Returns the advance width.
int DrawGlyph(LumaCanvas& canvas, LehmerRng& rng, int x, int baseline) {
  const int x_top = baseline - kXHeight;
  const int asc_top = baseline - kAscender;
  const int desc_bottom = baseline + kDescender;
  switch (rng.RandomInt(0, 9)) {
    case 0: {  // "l"
      canvas.Stem(x, asc_top, baseline);
      return 4;
    }
    case 1: {  // "i"
      canvas.Stem(x, x_top, baseline);
      canvas.Ink(x, x_top - 3, 1.0);
      canvas.Ink(x + 1, x_top - 3, 0.45);
      return 4;
    }
    case 2: {  // "n"
      canvas.Stem(x, x_top, baseline);
      canvas.Bar(x + 1, x + 6, x_top);
      canvas.Stem(x + 6, x_top + 1, baseline);
      return 9;
    }
    case 3: {  // "o"
      canvas.Bowl(x, x + 7, x_top, baseline);
      return 9;
    }
    case 4: {  // "e"
      canvas.Bowl(x, x + 7, x_top, baseline);
      canvas.Bar(x + 1, x + 6, x_top + kXHeight / 2);
      return 9;
    }
    case 5: {  // "d"
      canvas.Bowl(x, x + 6, x_top, baseline);
      canvas.Stem(x + 6, asc_top, baseline);
      return 9;
    }
    case 6: {  // "p"
      canvas.Stem(x, x_top, desc_bottom);
      canvas.Bowl(x + 1, x + 7, x_top, baseline);
      return 9;
    }
    case 7: {  // "t"
      canvas.Stem(x + 2, asc_top + 2, baseline);
      canvas.Bar(x, x + 6, x_top);
      return 7;
    }
    case 8: {  // "m"
      canvas.Stem(x, x_top, baseline);
      canvas.Bar(x + 1, x + 10, x_top);
      canvas.Stem(x + 5, x_top + 1, baseline);
      canvas.Stem(x + 10, x_top + 1, baseline);
      return 13;
    }
    default: {  // "s"/"z"-like
      canvas.Bar(x, x + 6, x_top);
      canvas.Bar(x, x + 6, x_top + kXHeight / 2);
      canvas.Bar(x, x + 6, baseline - 1);
      canvas.Ink(x, x_top + 1, 0.8);
      canvas.Ink(x + 5, baseline - 2, 0.8);
      return 8;
    }
  }
}

}  // namespace

TextDocumentContent::TextDocumentContent(const gfx::Size& frame_size,
                                         uint32_t seed)
    : frame_size_(frame_size),
      toolbar_height_(std::min(kToolbarHeight, frame_size.height() / 4)),
      document_height_(kNumPages * (kPageHeight + kPageGap)) {
  CHECK(!frame_size_.IsEmpty());
  CHECK_EQ(frame_size_.width() % 2, 0);
  CHECK_EQ(frame_size_.height() % 2, 0);
  GenerateDocument(seed);
}

TextDocumentContent::~TextDocumentContent() = default;

void TextDocumentContent::GenerateDocument(uint32_t seed) {
  LehmerRng rng(seed);
  const int width = frame_size_.width();

  // Toolbar: light background with a title, a menu row and a row of icons.
  toolbar_luma_.assign(width * toolbar_height_, kToolbarLuma);
  LumaCanvas toolbar(toolbar_luma_, width, toolbar_height_);
  for (int x = 60; x < std::min(width - 200, 400);) {
    x += DrawGlyph(toolbar, rng, x, 30) + 1;
  }
  for (int x = 60; x < std::min(width - 200, 520);) {
    x += DrawGlyph(toolbar, rng, x, 52) + (rng.RandomInt(0, 5) == 0 ? 12 : 1);
  }
  for (int x = 20; x + 16 < width - 20; x += 30) {
    toolbar.Bowl(x, x + 16, 74, 90);
    if (rng.RandomInt(0, 2) == 0) {
      toolbar.Bar(x + 4, x + 12, 82);
    }
  }
  toolbar.Fill(gfx::Rect(0, toolbar_height_ - 1, width, 1), 200);

  // Document: pages of text on a gray canvas.
  document_luma_.assign(width * document_height_, kCanvasLuma);
  LumaCanvas document(document_luma_, width, document_height_);
  const int page_width = std::min(kPageWidth, width - 2 * kPageGap);
  const int page_left = (width - page_width) / 2;
  const int margin = std::min(kPageMargin, page_width / 8);
  const int text_left = page_left + margin;
  const int text_right = page_left + page_width - margin;

  for (int page = 0; page < kNumPages; ++page) {
    const int page_top = page * (kPageHeight + kPageGap) + kPageGap / 2;
    document.Fill(gfx::Rect(page_left, page_top, page_width, kPageHeight),
                  kPageLuma);

    int lines_left_in_paragraph = rng.RandomInt(3, 9);
    for (int line_top = page_top + margin;
         line_top + kLinePitch <= page_top + kPageHeight - margin;
         line_top += kLinePitch) {
      if (lines_left_in_paragraph == 0) {
        // Blank line between paragraphs.
        lines_left_in_paragraph = rng.RandomInt(3, 9);
        continue;
      }
      --lines_left_in_paragraph;
      const int baseline = line_top + kBaselineOffset;
      // The last line of a paragraph is ragged.
      const int line_right = lines_left_in_paragraph == 0
                                 ? text_left + (text_right - text_left) *
                                                   rng.RandomInt(20, 90) / 100
                                 : text_right;
      int x = text_left;
      while (true) {
        const int word_length = rng.RandomInt(2, 10);
        if (x + word_length * 9 > line_right) {
          break;
        }
        for (int i = 0; i < word_length; ++i) {
          x += DrawGlyph(document, rng, x, baseline);
        }
        x += 4;  // Word space.
      }
    }
  }
}

scoped_refptr<media::VideoFrame> TextDocumentContent::Render(
    int scroll_offset,
    base::TimeDelta timestamp) const {
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, frame_size_, gfx::Rect(frame_size_),
      frame_size_, timestamp);
  CHECK(frame);
  RenderInto(scroll_offset, *frame);
  return frame;
}

void TextDocumentContent::RenderInto(int scroll_offset,
                                     media::VideoFrame& frame) const {
  CHECK_EQ(frame.format(), media::PIXEL_FORMAT_I420);
  CHECK_EQ(frame.visible_rect().size(), frame_size_);
  const int width = frame_size_.width();

  base::span<uint8_t> y_plane =
      frame.GetWritableVisiblePlaneData(media::VideoFrame::Plane::kY);
  const int y_stride = frame.stride(media::VideoFrame::Plane::kY);
  for (int row = 0; row < frame_size_.height(); ++row) {
    base::span<const uint8_t> source;
    if (row < toolbar_height_) {
      source = base::span(toolbar_luma_)
                   .subspan(static_cast<size_t>(row * width),
                            static_cast<size_t>(width));
    } else {
      int doc_row = (row - toolbar_height_ + scroll_offset) % document_height_;
      if (doc_row < 0) {
        doc_row += document_height_;
      }
      source = base::span(document_luma_)
                   .subspan(static_cast<size_t>(doc_row * width),
                            static_cast<size_t>(width));
    }
    y_plane
        .subspan(static_cast<size_t>(row * y_stride),
                 static_cast<size_t>(width))
        .copy_from(source);
  }

  for (auto plane :
       {media::VideoFrame::Plane::kU, media::VideoFrame::Plane::kV}) {
    base::span<uint8_t> data = frame.GetWritableVisiblePlaneData(plane);
    const int stride = frame.stride(plane);
    const int rows = frame_size_.height() / 2;
    const int cols = width / 2;
    for (int row = 0; row < rows; ++row) {
      std::ranges::fill(data.subspan(static_cast<size_t>(row * stride),
                                     static_cast<size_t>(cols)),
                        kNeutralChroma);
    }
  }
}

}  // namespace mirroring
