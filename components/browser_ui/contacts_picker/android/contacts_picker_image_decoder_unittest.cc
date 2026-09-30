// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/contacts_picker/android/contacts_picker_image_decoder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace browser_ui {
namespace {

TEST(ContactsPickerImageDecoderTest, NullBitmapReturnsNull) {
  SkBitmap empty;
  SkBitmap result = ScaleContactIconBitmap(empty, /*desired_size=*/36);
  EXPECT_TRUE(result.isNull());
}

TEST(ContactsPickerImageDecoderTest, ScalesNonSquareBitmapToSquareDimension) {
  SkBitmap src;
  ASSERT_TRUE(src.tryAllocN32Pixels(/*width=*/120, /*height=*/60));
  src.eraseColor(SK_ColorBLUE);

  SkBitmap result = ScaleContactIconBitmap(src, /*desired_size=*/36);
  ASSERT_FALSE(result.isNull());
  EXPECT_EQ(result.width(), 36);
  EXPECT_EQ(result.height(), 36);
  EXPECT_EQ(result.getColor(/*x=*/18, /*y=*/18), SK_ColorBLUE);
}

TEST(ContactsPickerImageDecoderTest, UnscaledWhenDesiredSizeZeroOrMatching) {
  SkBitmap src;
  ASSERT_TRUE(src.tryAllocN32Pixels(/*width=*/48, /*height=*/24));
  src.eraseColor(SK_ColorGREEN);

  // `desired_size` is 0 when the caller does not call
  // `FetchIconWorkerTask::setDesiredIconSize`, leaving the icon unscaled.
  SkBitmap unscaled = ScaleContactIconBitmap(src, /*desired_size=*/0);
  ASSERT_FALSE(unscaled.isNull());
  EXPECT_EQ(unscaled.width(), 48);
  EXPECT_EQ(unscaled.height(), 24);

  SkBitmap square_src;
  ASSERT_TRUE(square_src.tryAllocN32Pixels(/*width=*/36, /*height=*/36));
  square_src.eraseColor(SK_ColorRED);
  SkBitmap matching = ScaleContactIconBitmap(square_src, /*desired_size=*/36);
  ASSERT_FALSE(matching.isNull());
  EXPECT_EQ(matching.width(), 36);
  EXPECT_EQ(matching.height(), 36);
}

}  // namespace
}  // namespace browser_ui
