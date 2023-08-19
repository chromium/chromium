// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_UTILS_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "components/printing/browser/print_to_pdf/pdf_print_result.h"
#include "components/printing/common/print.mojom.h"
#include "printing/page_range.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace print_to_pdf {

// Converts textual representation of the page range to printing::PageRanges,
// page range error is returned as the PdfPrintResult variant case.
absl::variant<printing::PageRanges, PdfPrintResult> TextPageRangesToPageRanges(
    base::StringPiece page_range_text);

// Converts print settings to printing::mojom::PrintPagesParamsPtr,
// document error is returned as the string variant case.
absl::variant<printing::mojom::PrintPagesParamsPtr, std::string>
GetPrintPagesParams(const GURL& page_url,
                    absl::optional<bool> landscape,
                    absl::optional<bool> display_header_footer,
                    absl::optional<bool> print_background,
                    absl::optional<double> scale,
                    absl::optional<double> paper_width,
                    absl::optional<double> paper_height,
                    absl::optional<double> margin_top,
                    absl::optional<double> margin_bottom,
                    absl::optional<double> margin_left,
                    absl::optional<double> margin_right,
                    absl::optional<std::string> header_template,
                    absl::optional<std::string> footer_template,
                    absl::optional<bool> prefer_css_page_size,
                    absl::optional<bool> generate_tagged_pdf);

}  // namespace print_to_pdf

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_UTILS_H_
