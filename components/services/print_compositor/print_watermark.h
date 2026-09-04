// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_WATERMARK_H_
#define COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_WATERMARK_H_

#include <memory>

#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "components/services/print_compositor/print_compositor_impl.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"

static_assert(BUILDFLAG(ENTERPRISE_WATERMARK));

class SkCanvas;
class SkPicture;

namespace printing {

class PrintWatermark : public PrintCompositorImpl::Addon {
 public:
  // Creates a PrintWatermark instance. Returns nullptr if `watermark_block` is
  // invalid or if initialization fails.
  static std::unique_ptr<PrintWatermark> Create(
      watermark::mojom::WatermarkBlockPtr watermark_block);

  PrintWatermark(const PrintWatermark&) = delete;
  PrintWatermark& operator=(const PrintWatermark&) = delete;
  ~PrintWatermark() override;

  // PrintCompositorImpl::Addon:
  void OnDrawPage(SkCanvas* canvas, const SkSize& size) override;
  base::ReadOnlySharedMemoryRegion OnOverlayPdf(
      base::ReadOnlySharedMemoryRegion pdf_region) override;

  const watermark::mojom::WatermarkBlockPtr& block_for_testing() const {
    return watermark_block_;
  }

 private:
  PrintWatermark(watermark::mojom::WatermarkBlockPtr watermark_block,
                 sk_sp<SkPicture> picture);

  // Helper implementing the watermark overlay logic for `OnOverlayPdf()`.
  // Returns an invalid region if overlaying fails.
  base::ReadOnlySharedMemoryRegion OnOverlayPdfImpl(
      base::ReadOnlySharedMemoryRegion pdf_region);

  // The watermark block. Never null.
  watermark::mojom::WatermarkBlockPtr watermark_block_;

  // Cached deserialized SkPicture decoded once upon creation. Guaranteed to be
  // non-null.
  const sk_sp<SkPicture> cached_picture_;
};

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PRINT_COMPOSITOR_PRINT_WATERMARK_H_
