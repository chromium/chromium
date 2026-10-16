// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_

#include <cstddef>
#include <optional>

#include "base/time/time.h"

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
inline constexpr char kScreenshotCaptureStatusHistogram[] =
    "Glic.PageContextFetcher.GetEncodedScreenshot.Status2";

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

// Tracks the outcome of a screenshot capture request in `PageContextFetcher`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(ScreenshotCaptureStatus)
enum class ScreenshotCaptureStatus {
  // Screenshot capture was not requested or status is unknown.
  kUnknown = 0,
  // Screenshot was captured, optionally redacted, and encoded successfully.
  kSuccess = 1,
  // `RenderWidgetHostView` was null or surface was not available for copy.
  kNoViewOrSurface = 2,
  // `RenderWidgetHostView::CopyFromSurface` failed.
  kCopyFromSurfaceError = 3,
  // Paint preview screenshot service was unavailable, redaction params failed,
  // or `PageContentScreenshotService::RequestScreenshot` returned an error.
  kPaintPreviewError = 4,
  // Screenshot capture timed out before receiving the bitmap.
  kTimeout = 5,
  // Client-side redaction failed on the worker thread.
  kRedactionError = 6,
  // Image codec failed to encode the screenshot bitmap.
  kEncodingError = 7,
  kMaxValue = kEncodingError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:ScreenshotCaptureStatus)

void RecordPdfTextExtractionStatus(PdfTextExtractionStatus status);
void RecordScreenshotCaptureStatus(ScreenshotCaptureStatus status);

// Records `kPdfContentsRequestedHistogram` for a Glic PDF bytes request.
// `is_pdf_main_doc` is whether the main document is a PDF, and `pdf_found` is
// whether PDF contents were found.
void RecordPdfContentsRequested(bool is_pdf_main_doc, bool pdf_found);

// Records the PDF bytes extraction histograms for a completed extraction.
// `pdf_size_bytes` is recorded only if set.
void RecordPdfBytesExtraction(bool is_top_level_pdf,
                              base::TimeDelta latency,
                              std::optional<size_t> pdf_size_bytes,
                              bool size_limit_exceeded);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_PAGE_CONTEXT_FETCHER_METRICS_H_
