// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/eye_dropper/eye_dropper_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace eye_dropper {
namespace {

class BitmapDesktopFrame : public webrtc::DesktopFrame {
 public:
  BitmapDesktopFrame(const SkBitmap& bitmap, int width)
      : DesktopFrame(webrtc::DesktopSize(width, bitmap.height()),
                     bitmap.rowBytes(),
                     webrtc::FOURCC_ARGB,
                     static_cast<uint8_t*>(bitmap.getPixels()),
                     nullptr) {}
};

TEST(EyeDropperViewTest, CreateBitmapFromTightlyPackedFrame) {
  SkBitmap source;
  source.allocN32Pixels(2, 3, true);
  source.eraseColor(SK_ColorRED);
  source.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 1, 2, 1));
  source.erase(SK_ColorGREEN, SkIRect::MakeXYWH(0, 2, 2, 1));
  BitmapDesktopFrame frame(source, source.width());

  SkBitmap bitmap = EyeDropperView::CreateBitmapFromFrame(frame);

  EXPECT_EQ(bitmap.width(), 2);
  EXPECT_EQ(bitmap.height(), 3);
  EXPECT_TRUE(bitmap.isImmutable());
  for (int x = 0; x < bitmap.width(); ++x) {
    EXPECT_EQ(bitmap.getColor(x, 0), SK_ColorRED);
    EXPECT_EQ(bitmap.getColor(x, 1), SK_ColorBLUE);
    EXPECT_EQ(bitmap.getColor(x, 2), SK_ColorGREEN);
  }

  source.eraseColor(SK_ColorBLACK);
  EXPECT_EQ(bitmap.getColor(0, 0), SK_ColorRED);
}

TEST(EyeDropperViewTest, CreateBitmapFromPaddedFrame) {
  SkBitmap source;
  source.allocN32Pixels(4, 3, true);
  source.eraseColor(SK_ColorRED);
  source.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 1, 4, 1));
  source.erase(SK_ColorGREEN, SkIRect::MakeXYWH(0, 2, 4, 1));
  BitmapDesktopFrame frame(source, 2);
  ASSERT_GT(frame.stride(),
            frame.size().width() * webrtc::DesktopFrame::kBytesPerPixel);

  SkBitmap bitmap = EyeDropperView::CreateBitmapFromFrame(frame);

  EXPECT_EQ(bitmap.width(), 2);
  EXPECT_EQ(bitmap.height(), 3);
  EXPECT_TRUE(bitmap.isImmutable());
  for (int x = 0; x < bitmap.width(); ++x) {
    EXPECT_EQ(bitmap.getColor(x, 0), SK_ColorRED);
    EXPECT_EQ(bitmap.getColor(x, 1), SK_ColorBLUE);
    EXPECT_EQ(bitmap.getColor(x, 2), SK_ColorGREEN);
  }
}

}  // namespace
}  // namespace eye_dropper
