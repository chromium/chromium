// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_RESULT_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_RESULT_H_

#include <string>

namespace print_to_pdf {

enum class PdfPrintResult {
  PRINT_SUCCESS,
  PRINTING_FAILED,
  INVALID_PRINTER_SETTINGS,
  INVALID_MEMORY_HANDLE,
  METAFILE_MAP_ERROR,
  SIMULTANEOUS_PRINT_ACTIVE,
  PAGE_RANGE_SYNTAX_ERROR,
  PAGE_RANGE_INVALID_RANGE,
  PAGE_COUNT_EXCEEDED,

};

std::string PdfPrintResultToString(PdfPrintResult result);

}  // namespace print_to_pdf

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_RESULT_H_
