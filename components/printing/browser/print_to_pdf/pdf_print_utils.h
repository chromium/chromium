// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_UTILS_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "components/printing/browser/print_to_pdf/pdf_print_result.h"
#include "components/printing/common/print.mojom.h"
#include "printing/page_range.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace print_to_pdf {

// Converts textual representation of the page range to printing::PageRanges,
// page range error is returned as the PdfPrintResult variant case.
absl::variant<printing::PageRanges, PdfPrintResult> TextPageRangesToPageRanges(
    std::string_view page_range_text);

// Converts print settings to printing::mojom::PrintPagesParamsPtr,
// document error is returned as the string variant case.
absl::variant<printing::mojom::PrintPagesParamsPtr, std::string>
GetPrintPagesParams(const GURL& page_url,
                    std::optional<bool> landscape,
                    std::optional<bool> display_header_footer,
                    std::optional<bool> print_background,
                    std::optional<double> scale,
                    std::optional<double> paper_width,
                    std::optional<double> paper_height,
                    std::optional<double> margin_top,
                    std::optional<double> margin_bottom,
                    std::optional<double> margin_left,
                    std::optional<double> margin_right,
                    std::optional<std::string> header_template,
                    std::optional<std::string> footer_template,
                    std::optional<bool> prefer_css_page_size,
                    std::optional<bool> generate_tagged_pdf,
                    std::optional<bool> generate_document_outline);

}  // namespace print_to_pdf

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_TO_PDF_PDF_PRINT_UTILS_H_
