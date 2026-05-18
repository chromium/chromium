// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_

namespace page_content_annotations {

inline constexpr char kPdfTextExtractionLatencyHistogram[] =
    "Glic.PageContextFetcher.PdfTextExtraction.Time";
inline constexpr char kPdfTextExtractionSizeHistogram[] =
    "Glic.PageContextFetcher.PdfTextExtraction.Size";
inline constexpr char kPdfTextExtractionStatusHistogram[] =
    "Glic.PageContextFetcher.PdfTextExtraction.Status";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(PdfTextExtractionStatus)
enum class PdfTextExtractionStatus {
  // Extraction completed and returned non-empty text.
  kSuccess = 0,
  // Extraction completed but returned empty text. Possible scenarios:
  // - PDF is blank. An empty text is the expected behavior.
  // - PDF contains scanned text, but the OCR does not complete.
  // - PDF contains scanned text, and the OCR completes but no text result.
  // - PDF rendering fails.
  kEmptyText = 1,
  // Extraction requested but the document is not a PDF.
  kNotPdf = 2,
  // Extraction requested but extraction provider `PDFDocumentHelper` was not
  // available.
  kPdfExtractionNotAvailable = 3,
  // Extraction requested but the PDF document was not fully loaded.
  kPdfDocumentNotLoaded = 4,
  kMaxValue = kPdfDocumentNotLoaded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:PdfTextExtractionStatus)

void RecordPdfTextExtractionStatus(PdfTextExtractionStatus status);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_
