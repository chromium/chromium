// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_METRICS_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_METRICS_H_

namespace page_content_annotations {

inline constexpr char kPageContentExtractionRequestTypeHistogram[] =
    "OptimizationGuide.PageContentExtraction.RequestType";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// LINT.IfChange(ExtractionRequestType)
enum class ExtractionRequestType {
  kAnnotatedPageContent = 0,
  kPDFText = 1,
  kPDFPageCount = 2,
  kMaxValue = kPDFPageCount,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:PageContentExtractionRequestType)

void RecordRequestType(ExtractionRequestType request_type);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_ANNOTATE_PAGE_CONTENT_REQUEST_METRICS_H_
