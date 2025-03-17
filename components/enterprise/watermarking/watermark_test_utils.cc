// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/watermark_test_utils.h"

#include <utility>

#include "base/memory/shared_memory_mapping.h"
#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "components/enterprise/watermarking/watermark.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace enterprise_watermark {

watermark::mojom::WatermarkBlockPtr MakeTestWatermarkBlock(
    const std::string& watermark_text,
    const SkSize watermark_size) {
  // Initialize text blob
  static constexpr SkScalar kTextSize = 30.0f;
  SkFont font(skia::DefaultTypeface(), kTextSize, 1.0f, 0.0f);
  sk_sp<SkTextBlob> blob =
      SkTextBlob::MakeFromString(watermark_text.c_str(), font);

  // Draw onto SkPicture-backed SkCanvas
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(
      SkRect{SkRect::MakeWH(watermark_size.fWidth, watermark_size.fHeight)});
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  canvas->drawTextBlob(blob.get(), 0.0f, 0.0f, paint);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

  // Serialize SkPicture
  SkDynamicMemoryWStream stream;
  SkSerialProcs procs;
  picture->serialize(&stream, &procs);
  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(stream.bytesWritten());
  if (!region_mapping.IsValid()) {
    return nullptr;
  }
  stream.copyTo(region_mapping.mapping.memory());

  // Measure string dimensions
  SkScalar text_width = font.measureText(
      watermark_text.c_str(), watermark_text.size(), SkTextEncoding::kUTF8);

  // Construct test data
  return watermark::mojom::WatermarkBlockPtr(
      std::in_place, std::move(region_mapping.region), text_width, kTextSize);
}

}  // namespace enterprise_watermark
