// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_RESULT_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_RESULT_H_

#include <string>

namespace print_to_pdf {

enum class PdfPrintResult {
  kPrintSuccess,
  kPrintFailure,
  kInvalidSharedMemoryRegion,
  kInvalidSharedMemoryMapping,
  kPageRangeSyntaxError,
  kPageRangeInvalidRange,
  kPageCountExceeded,
  kPrintingInProgress,
};

std::string PdfPrintResultToString(PdfPrintResult result);

}  // namespace print_to_pdf

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_RESULT_H_
