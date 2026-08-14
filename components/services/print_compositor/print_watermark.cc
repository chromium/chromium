// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/print_compositor/print_watermark.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "components/enterprise/watermarking/watermark.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/skia_span_util.h"

namespace printing {

PrintWatermark::PrintWatermark(
    watermark::mojom::WatermarkBlockPtr watermark_block)
    : watermark_block_(std::move(watermark_block)) {
  CHECK(watermark_block_);
}

PrintWatermark::~PrintWatermark() = default;

void PrintWatermark::OnDrawPage(SkCanvas* canvas, const SkSize& size) {
  base::ReadOnlySharedMemoryMapping mapping =
      watermark_block_->serialized_skpicture.Map();
  if (!mapping.IsValid()) {
    LOG(ERROR)
        << "Error serializing the watermark block received from the browser";
    return;
  }
  auto skpicture_span = mapping.GetMemoryAsSpan<uint8_t>();
  SkMemoryStream stream(gfx::MakeSkDataFromSpanWithoutCopy(skpicture_span));
  sk_sp<SkPicture> picture = SkPicture::MakeFromStream(&stream);

  enterprise_watermark::DrawWatermark(canvas, picture.get(),
                                      watermark_block_->width,
                                      watermark_block_->height, size);
}

}  // namespace printing
