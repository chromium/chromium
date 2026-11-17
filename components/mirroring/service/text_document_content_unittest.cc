// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/text_document_content.h"

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace mirroring {
namespace {

std::vector<uint8_t> ExtractYPlaneRows(const media::VideoFrame& frame,
                                       int start_row,
                                       int num_rows) {
  const int width = frame.visible_rect().width();
  const int stride = frame.stride(media::VideoFrame::Plane::kY);
  base::span<const uint8_t> y_plane =
      frame.GetVisiblePlaneData(media::VideoFrame::Plane::kY);
  std::vector<uint8_t> out;
  out.reserve(static_cast<size_t>(width) * num_rows);
  for (int row = start_row; row < start_row + num_rows; ++row) {
    auto row_span = y_plane.subspan(static_cast<size_t>(row * stride),
                                    static_cast<size_t>(width));
    out.insert(out.end(), row_span.begin(), row_span.end());
  }
  return out;
}

TEST(TextDocumentContentTest,
     RendersDeterministicI420WithFixedToolbarAndNeutralChroma) {
  const gfx::Size frame_size(640, 480);
  const TextDocumentContent first(frame_size, /*seed=*/7);
  const TextDocumentContent second(frame_size, /*seed=*/7);

  scoped_refptr<media::VideoFrame> frame1 =
      first.Render(/*scroll_offset=*/0, base::Milliseconds(42));
  scoped_refptr<media::VideoFrame> frame2 =
      second.Render(/*scroll_offset=*/0, base::Milliseconds(42));

  ASSERT_TRUE(frame1);
  ASSERT_TRUE(frame2);
  EXPECT_EQ(frame1->format(), media::PIXEL_FORMAT_I420);
  EXPECT_EQ(frame1->visible_rect().size(), frame_size);
  EXPECT_EQ(frame1->timestamp(), base::Milliseconds(42));

  EXPECT_EQ(ExtractYPlaneRows(*frame1, 0, frame_size.height()),
            ExtractYPlaneRows(*frame2, 0, frame_size.height()));

  // Verify neutral chroma (128) across U and V planes.
  for (auto plane :
       {media::VideoFrame::Plane::kU, media::VideoFrame::Plane::kV}) {
    base::span<const uint8_t> data = frame1->GetVisiblePlaneData(plane);
    const int stride = frame1->stride(plane);
    const int rows = frame_size.height() / 2;
    const int cols = frame_size.width() / 2;
    for (int row = 0; row < rows; ++row) {
      auto row_span = data.subspan(static_cast<size_t>(row * stride),
                                   static_cast<size_t>(cols));
      EXPECT_TRUE(
          std::ranges::all_of(row_span, [](uint8_t v) { return v == 128; }));
    }
  }

  // Scrolling keeps the top toolbar fixed while updating the document area,
  // and the document area includes both dark text ink and white page luma.
  const int toolbar_height = first.toolbar_height();
  const int body_rows = frame_size.height() - toolbar_height;
  ASSERT_GT(toolbar_height, 0);
  ASSERT_GT(body_rows, 0);

  const std::vector<uint8_t> body_unscrolled =
      ExtractYPlaneRows(*frame1, toolbar_height, body_rows);
  EXPECT_TRUE(std::ranges::contains(body_unscrolled, uint8_t{235}));
  EXPECT_TRUE(std::ranges::any_of(body_unscrolled,
                                  [](uint8_t luma) { return luma < 100; }));

  scoped_refptr<media::VideoFrame> scrolled =
      first.Render(/*scroll_offset=*/80, base::Milliseconds(84));
  EXPECT_EQ(ExtractYPlaneRows(*frame1, 0, toolbar_height),
            ExtractYPlaneRows(*scrolled, 0, toolbar_height));
  EXPECT_NE(body_unscrolled,
            ExtractYPlaneRows(*scrolled, toolbar_height, body_rows));
}

TEST(TextDocumentContentTest,
     WrapsScrollOffsetAcrossDocumentHeightAndNegativeOffsets) {
  const gfx::Size frame_size(640, 480);
  const TextDocumentContent content(frame_size, /*seed=*/11);
  const int doc_height = content.document_height();
  ASSERT_GT(doc_height, 0);

  const int base_offset = 123;
  scoped_refptr<media::VideoFrame> base_frame =
      content.Render(base_offset, base::Milliseconds(10));
  scoped_refptr<media::VideoFrame> wrapped_positive =
      content.Render(base_offset + 2 * doc_height, base::Milliseconds(20));
  scoped_refptr<media::VideoFrame> wrapped_negative =
      content.Render(base_offset - 2 * doc_height, base::Milliseconds(30));

  const std::vector<uint8_t> expected_y =
      ExtractYPlaneRows(*base_frame, 0, frame_size.height());
  EXPECT_EQ(expected_y,
            ExtractYPlaneRows(*wrapped_positive, 0, frame_size.height()));
  EXPECT_EQ(expected_y,
            ExtractYPlaneRows(*wrapped_negative, 0, frame_size.height()));
}

}  // namespace
}  // namespace mirroring
