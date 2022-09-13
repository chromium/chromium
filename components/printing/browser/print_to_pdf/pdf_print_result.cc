// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_result.h"

namespace print_to_pdf {

std::string PdfPrintResultToString(PdfPrintResult result) {
  switch (result) {
    case PdfPrintResult::kPrintSuccess:
      return std::string();  // no error message
    case PdfPrintResult::kPrintFailure:
      return "Printing failed";
    case PdfPrintResult::kInvalidSharedMemoryRegion:
      return "Invalid shared memory region";
    case PdfPrintResult::kInvalidSharedMemoryMapping:
      return "Invalid shared memory mapping";
    case PdfPrintResult::kPageRangeSyntaxError:
      return "Page range syntax error";
    case PdfPrintResult::kPageRangeInvalidRange:
      return "Page range is invalid (start > end)";
    case PdfPrintResult::kPageCountExceeded:
      return "Page range exceeds page count";
    case PdfPrintResult::kPrintingInProgress:
      return "Page is already being printed";
  }
}

}  // namespace print_to_pdf
