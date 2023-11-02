// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_PAGE_LOAD_METRICS_TEST_UTIL_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_PAGE_LOAD_METRICS_TEST_UTIL_H_

#include <stdint.h>

#include <vector>

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

// Helper that fills in any timing fields that page load metrics requires but
// that are currently missing.
void PopulateRequiredTimingFields(
    page_load_metrics::mojom::PageLoadTiming* inout_timing);

// Sets the experimental LCP values to be equal to the non-experimental
// counterparts, which are assumed to be already set.
void PopulateExperimentalLCP(page_load_metrics::mojom::PaintTimingPtr& timing);

// Helper that creates a resource update mojo.
page_load_metrics::mojom::ResourceDataUpdatePtr CreateResource(
    bool was_cached,
    int64_t delta_bytes,
    int64_t encoded_body_length,
    int64_t decoded_body_length,
    bool is_complete);

// Helper that returns a sample resource data update using a variety of
// configurations.
std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>
GetSampleResourceDataUpdateForTesting(int64_t resource_size);

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_TEST_PAGE_LOAD_METRICS_TEST_UTIL_H_
