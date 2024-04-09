// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PDF_PDF_THUMBNAILER_H_
#define CHROME_SERVICES_PDF_PDF_THUMBNAILER_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/services/pdf/public/mojom/pdf_thumbnailer.mojom.h"

namespace pdf {

// Implements mojom interface for generating thumbnails for PDF files.
// The PDF file needs to be read and stored in a shared memory region.
// Size of the thumbnail to be generated is specified via ThumbParams.
class PdfThumbnailer : public pdf::mojom::PdfThumbnailer {
 public:
  PdfThumbnailer();
  ~PdfThumbnailer() override;

  PdfThumbnailer(const PdfThumbnailer&) = delete;
  PdfThumbnailer& operator=(const PdfThumbnailer&) = delete;

  // pdf::mojom::PdfThumbnailer:
  void GetThumbnail(pdf::mojom::ThumbParamsPtr params,
                    base::ReadOnlySharedMemoryRegion pdf_region,
                    GetThumbnailCallback callback) override;
  void SetUseSkiaRendererPolicy(bool use_skia) override;
};

}  // namespace pdf

#endif  // CHROME_SERVICES_PDF_PDF_THUMBNAILER_H_
