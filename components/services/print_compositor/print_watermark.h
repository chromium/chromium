// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_WATERMARK_H_
#define COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_WATERMARK_H_

#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "third_party/skia/include/core/SkSize.h"

static_assert(BUILDFLAG(ENTERPRISE_WATERMARK));

class SkCanvas;

namespace printing {

class PrintWatermark {
 public:
  PrintWatermark();
  PrintWatermark(const PrintWatermark&) = delete;
  PrintWatermark& operator=(const PrintWatermark&) = delete;
  ~PrintWatermark();

  void SetBlock(watermark::mojom::WatermarkBlockPtr watermark_block);
  void Draw(SkCanvas* canvas, const SkSize& size) const;

  const watermark::mojom::WatermarkBlockPtr& block_for_testing() const {
    return watermark_block_;
  }

 private:
  // The watermark block. The special value `nullptr` indicates that there is no
  // watermark.
  watermark::mojom::WatermarkBlockPtr watermark_block_;
};

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_WATERMARK_H_
