// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_FLATTENER_H_
#define CHROME_SERVICES_PRINTING_PDF_FLATTENER_H_

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"

namespace printing {

class PdfFlattener : public printing::mojom::PdfFlattener {
 public:
  PdfFlattener();
  ~PdfFlattener() override;

  // printing::mojom::PdfFlattener:
  void FlattenPdf(base::ReadOnlySharedMemoryRegion src_pdf_region,
                  FlattenPdfCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PdfFlattener);
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_FLATTENER_H_
