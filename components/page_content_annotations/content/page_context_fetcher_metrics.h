// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_

namespace page_content_annotations {

// TODO(b/562182406): Once embedded PDF text extraction is supported, create
// corresponding metrics.
// LINT.IfChange(PdfExtractionHistogramNames)
inline constexpr char kPdfBytesTopLevelLatencyHistogram[] =
    "Glic.PageContextFetcher.PdfBytesExtraction.TopLevel.Time";
inline constexpr char kPdfBytesTopLevelSizeHistogram[] =
    "Glic.PageContextFetcher.PdfBytesExtraction.TopLevel.Size";
inline constexpr char kPdfBytesTopLevelSizeLimitExceededHistogram[] =
    "Glic.PageContextFetcher.PdfBytesExtraction.TopLevel.SizeLimitExceeded";

inline constexpr char kPdfBytesEmbeddedLatencyHistogram[] =
    "Glic.PageContextFetcher.PdfBytesExtraction.Embedded.Time";
inline constexpr char kPdfBytesEmbeddedSizeHistogram[] =
    "Glic.PageContextFetcher.PdfBytesExtraction.Embedded.Size";
inline constexpr char kPdfBytesEmbeddedSizeLimitExceededHistogram[] =
    "Glic.PageContextFetcher.PdfBytesExtraction.Embedded.SizeLimitExceeded";

inline constexpr char kPdfTextTopLevelLatencyHistogram[] =
    "Glic.PageContextFetcher.PdfTextExtraction.TopLevel.Time";
inline constexpr char kPdfTextTopLevelSizeHistogram[] =
    "Glic.PageContextFetcher.PdfTextExtraction.TopLevel.Size";
inline constexpr char kPdfTextTopLevelSizeLimitExceededHistogram[] =
    "Glic.PageContextFetcher.PdfTextExtraction.TopLevel.SizeLimitExceeded";
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/histograms.xml:PDFExtractionMode)

inline constexpr char kPdfTextExtractionStatusHistogram[] =
    "Glic.PageContextFetcher.PdfTextExtraction.Status";
inline constexpr char kPdfContentsRequestedHistogram[] =
    "Glic.TabContext.PdfContentsRequested";

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

// Combination of tracked states for when a PDF contents request is made by
// Glic.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(PdfRequestStates)
enum class PdfRequestStates {
  kPdfMainDoc_PdfFound = 0,
  kPdfMainDoc_PdfNotFound = 1,
  kNonPdfMainDoc_PdfFound = 2,
  kNonPdfMainDoc_PdfNotFound = 3,
  kMaxValue = kNonPdfMainDoc_PdfNotFound,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:PdfRequestStates)

void RecordPdfTextExtractionStatus(PdfTextExtractionStatus status);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_
