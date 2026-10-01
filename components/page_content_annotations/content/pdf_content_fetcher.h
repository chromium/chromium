// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PDF_CONTENT_FETCHER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PDF_CONTENT_FETCHER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "pdf/mojom/pdf.mojom.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace pdf {
class PDFDocumentHelper;
}  // namespace pdf

namespace page_content_annotations {

// Fetches PDF content (bytes or text) for `web_contents` using the PDF
// extension/PDFDocumentHelper and invokes `callback` with the result.
void FetchPdfContentForWebContents(content::WebContents& web_contents,
                                   const PdfOptions& options,
                                   FetchPdfContentResultCallback callback);

// Finds the `PDFDocumentHelper` of the PDF extraction candidate in
// `WebContents`. Exposed for testing.
pdf::PDFDocumentHelper* GetPDFExtractionCandidateForTesting(
    content::WebContents& contents);

// Converts extracted PDF bytes into a `PdfResult`, enforcing `pdf_size_limit`,
// recording PDF bytes extraction histograms, and invoking `callback`.
void ConvertPdfBytesToResultForTesting(
    url::Origin pdf_origin,
    bool is_top_level_pdf,
    uint32_t pdf_size_limit,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& pdf_bytes,
    uint32_t page_count,
    FetchPdfContentResultCallback callback);

// Converts extracted UTF-16 PDF page text into a UTF-8 `PdfResult`, truncating
// to `text_byte_limit`, recording PDF text extraction histograms, and invoking
// `callback`.
void ConvertPdfTextToResultForTesting(url::Origin pdf_origin,
                                      uint32_t text_byte_limit,
                                      const std::u16string& text,
                                      FetchPdfContentResultCallback callback);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PDF_CONTENT_FETCHER_H_
