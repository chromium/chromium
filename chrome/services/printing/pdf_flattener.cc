// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/pdf_flattener.h"

#include <utility>

#include "base/containers/span.h"
#include "base/memory/shared_memory_mapping.h"
#include "pdf/pdf.h"

namespace printing {

PdfFlattener::PdfFlattener() = default;

PdfFlattener::~PdfFlattener() = default;

void PdfFlattener::FlattenPdf(base::ReadOnlySharedMemoryRegion src_pdf_region,
                              FlattenPdfCallback callback) {
  base::ReadOnlySharedMemoryMapping pdf_mapping = src_pdf_region.Map();
  if (!pdf_mapping.IsValid()) {
    std::move(callback).Run(base::ReadOnlySharedMemoryRegion());
    return;
  }

  auto input_pdf_buffer = pdf_mapping.GetMemoryAsSpan<const uint8_t>();
  std::vector<uint8_t> output_pdf_buffer =
      chrome_pdf::CreateFlattenedPdf(input_pdf_buffer);
  if (output_pdf_buffer.empty()) {
    std::move(callback).Run(base::ReadOnlySharedMemoryRegion());
    return;
  }

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(output_pdf_buffer.size());
  if (!region_mapping.IsValid()) {
    std::move(callback).Run(std::move(region_mapping.region));
    return;
  }

  memcpy(region_mapping.mapping.memory(), output_pdf_buffer.data(),
         output_pdf_buffer.size());
  std::move(callback).Run(std::move(region_mapping.region));
}

}  // namespace printing
