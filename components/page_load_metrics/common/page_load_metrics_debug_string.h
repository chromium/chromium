// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_DEBUG_STRING_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_DEBUG_STRING_H_

#include <string>

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"

namespace page_load_metrics {

std::string DebugString(const mojom::DocumentTiming& timing);

std::string DebugString(const mojom::LcpResourceLoadTimings& timings);

std::string DebugString(const mojom::LargestContentfulPaintTiming& lcp);

std::string DebugString(const mojom::PaintTiming& timing);

std::string DebugString(const mojom::ParseTiming& timing);

std::string DebugString(const mojom::InteractiveTiming& timing);

std::string DebugString(const mojom::DomainLookupTiming& timing);

std::string DebugString(const mojom::PageLoadTiming& timing);

std::string DebugString(const mojom::BackForwardCacheTiming& timing);

std::string DebugString(
    const mojom::SoftNavigationMetrics& soft_navigation_metrics);

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_DEBUG_STRING_H_
