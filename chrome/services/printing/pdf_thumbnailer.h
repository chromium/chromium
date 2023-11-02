// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_THUMBNAILER_H_
#define CHROME_SERVICES_PRINTING_PDF_THUMBNAILER_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/services/printing/public/mojom/pdf_thumbnailer.mojom.h"

namespace printing {

// Implements mojom interface for generating thumbnails for PDF files.
// The PDF file needs to be read and stored in a shared memory region.
// Size of the thumbnail to be generated is specified via ThumbParams.
class PdfThumbnailer : public printing::mojom::PdfThumbnailer {
 public:
  PdfThumbnailer();
  ~PdfThumbnailer() override;

  PdfThumbnailer(const PdfThumbnailer&) = delete;
  PdfThumbnailer& operator=(const PdfThumbnailer&) = delete;

  // printing::mojom::PdfThumbnailer:
  void GetThumbnail(printing::mojom::ThumbParamsPtr params,
                    base::ReadOnlySharedMemoryRegion pdf_region,
                    GetThumbnailCallback callback) override;

  // The maximum width of a thumbnail we accept. If the specified width
  // exceeds the maximum, an empty, invalid bitmap is returned.
  constexpr static int kMaxWidth = 512;

  // The maximum height of a thumbnail we accept. If the specified height
  // exceeds the maximum, an empty, invalid bitmap is returned.
  constexpr static int kMaxHeight = 512;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_THUMBNAILER_H_
