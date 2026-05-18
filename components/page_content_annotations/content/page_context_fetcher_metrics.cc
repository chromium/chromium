// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace page_content_annotations {

void RecordPdfTextExtractionStatus(PdfTextExtractionStatus status) {
  base::UmaHistogramEnumeration(kPdfTextExtractionStatusHistogram, status);
}

}  // namespace page_content_annotations
