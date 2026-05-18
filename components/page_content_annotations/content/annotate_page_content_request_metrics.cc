// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/annotate_page_content_request_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace page_content_annotations {

void RecordRequestType(ExtractionRequestType request_type) {
  base::UmaHistogramEnumeration(kPageContentExtractionRequestTypeHistogram,
                                request_type);
}

}  // namespace page_content_annotations
