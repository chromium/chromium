// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/convert_to_java_bitmap.h"

#include "base/functional/bind.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"

namespace paint_preview {

TEST(PaintPreviewConvertToJavaBitmap, Success) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(50, 100);
  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorBLACK);
  bool called = false;

  ConvertToJavaBitmap(
      base::BindOnce(
          [](bool* called, JavaBitmapResult result) {
            *called = true;
            EXPECT_EQ(result.status,
                      mojom::PaintPreviewCompositor::BitmapStatus::kSuccess);
            EXPECT_EQ(result.bytes, 4U * 50U * 100U);
            EXPECT_TRUE(result.java_bitmap);
          },
          base::Unretained(&called)),
      mojom::PaintPreviewCompositor::BitmapStatus::kSuccess, bitmap);

  ASSERT_TRUE(called);
}

TEST(PaintPreviewConvertToJavaBitmap, MojoFailure) {
  SkBitmap bitmap;
  bool called = false;

  ConvertToJavaBitmap(
      base::BindOnce(
          [](bool* called, JavaBitmapResult result) {
            *called = true;
            EXPECT_EQ(
                result.status,
                mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame);
            EXPECT_EQ(result.bytes, 0U);
            EXPECT_FALSE(result.java_bitmap);
          },
          base::Unretained(&called)),
      mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame, bitmap);

  ASSERT_TRUE(called);
}

TEST(PaintPreviewConvertToJavaBitmap, AssumeAllocFailed) {
  SkBitmap bitmap;
  bool called = false;

  ConvertToJavaBitmap(
      base::BindOnce(
          [](bool* called, JavaBitmapResult result) {
            *called = true;
            EXPECT_EQ(
                result.status,
                mojom::PaintPreviewCompositor::BitmapStatus::kAllocFailed);
            EXPECT_EQ(result.bytes, 0U);
            EXPECT_FALSE(result.java_bitmap);
          },
          base::Unretained(&called)),
      mojom::PaintPreviewCompositor::BitmapStatus::kSuccess, bitmap);

  ASSERT_TRUE(called);
}

}  // namespace paint_preview
