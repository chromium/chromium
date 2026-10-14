// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace page_content_annotations {

void RecordPdfTextExtractionStatus(PdfTextExtractionStatus status) {
  base::UmaHistogramEnumeration(kPdfTextExtractionStatusHistogram, status);
}

void RecordScreenshotCaptureStatus(ScreenshotCaptureStatus status) {
  base::UmaHistogramEnumeration(kScreenshotCaptureStatusHistogram, status);
}

void RecordPdfContentsRequested(bool is_pdf_main_doc, bool pdf_found) {
  PdfRequestStates state;
  if (is_pdf_main_doc) {
    state = pdf_found ? PdfRequestStates::kPdfMainDoc_PdfFound
                      : PdfRequestStates::kPdfMainDoc_PdfNotFound;
  } else {
    state = pdf_found ? PdfRequestStates::kNonPdfMainDoc_PdfFound
                      : PdfRequestStates::kNonPdfMainDoc_PdfNotFound;
  }
  base::UmaHistogramEnumeration(kPdfContentsRequestedHistogram, state);
}

void RecordPdfBytesExtraction(bool is_top_level_pdf,
                              base::TimeDelta latency,
                              std::optional<size_t> pdf_size_bytes,
                              bool size_limit_exceeded) {
  // The sample is recorded in milliseconds.
  base::UmaHistogramTimes(is_top_level_pdf ? kPdfBytesTopLevelLatencyHistogram
                                           : kPdfBytesEmbeddedLatencyHistogram,
                          latency);

  // The sample is recorded in KB. Bytes extraction size is capped at 64MB by
  // default.
  if (pdf_size_bytes.has_value()) {
    base::UmaHistogramCounts100000(is_top_level_pdf
                                       ? kPdfBytesTopLevelSizeHistogram
                                       : kPdfBytesEmbeddedSizeHistogram,
                                   *pdf_size_bytes / 1024);
  }

  base::UmaHistogramBoolean(is_top_level_pdf
                                ? kPdfBytesTopLevelSizeLimitExceededHistogram
                                : kPdfBytesEmbeddedSizeLimitExceededHistogram,
                            size_limit_exceeded);
}

}  // namespace page_content_annotations
