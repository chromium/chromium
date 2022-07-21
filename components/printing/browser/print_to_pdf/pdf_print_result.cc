// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_to_pdf/pdf_print_result.h"

namespace print_to_pdf {

std::string PdfPrintResultToString(PdfPrintResult result) {
  switch (result) {
    case PdfPrintResult::PRINT_SUCCESS:
      return std::string();  // no error message
    case PdfPrintResult::PRINTING_FAILED:
      return "Printing failed";
    case PdfPrintResult::INVALID_PRINTER_SETTINGS:
      return "Show invalid printer settings error";
    case PdfPrintResult::INVALID_MEMORY_HANDLE:
      return "Invalid memory handle";
    case PdfPrintResult::METAFILE_MAP_ERROR:
      return "Map to shared memory error";
    case PdfPrintResult::SIMULTANEOUS_PRINT_ACTIVE:
      return "The previous printing job hasn't finished";
    case PdfPrintResult::PAGE_RANGE_SYNTAX_ERROR:
      return "Page range syntax error";
    case PdfPrintResult::PAGE_RANGE_INVALID_RANGE:
      return "Page range is invalid (start > end)";
    case PdfPrintResult::PAGE_COUNT_EXCEEDED:
      return "Page range exceeds page count";
  }
}

}  // namespace print_to_pdf
