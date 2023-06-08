// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_SOFT_NAVIGATION_METRICS_TYPE_CONVERTER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_SOFT_NAVIGATION_METRICS_TYPE_CONVERTER_H_

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/common/performance/performance_timeline_constants.h"

namespace mojo {
// TypeConverter to translate from blink::SoftNavigationMetrics to
// to page_load_metrics::mojom::SoftNavigationMetrics.
template <>
struct TypeConverter<
    mojo::StructPtr<page_load_metrics::mojom::SoftNavigationMetrics>,
    blink::SoftNavigationMetrics> {
  static mojo::StructPtr<page_load_metrics::mojom::SoftNavigationMetrics>
  Convert(blink::SoftNavigationMetrics soft_navigation_metrics) {
    auto mojom_soft_navigation_metrics =
        page_load_metrics::mojom::SoftNavigationMetrics::New();

    mojom_soft_navigation_metrics->count = soft_navigation_metrics.count;

    mojom_soft_navigation_metrics->start_time =
        soft_navigation_metrics.start_time;

    mojom_soft_navigation_metrics->navigation_id =
        soft_navigation_metrics.navigation_id;

    return mojom_soft_navigation_metrics;
  }
};
}  // namespace mojo
#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_SOFT_NAVIGATION_METRICS_TYPE_CONVERTER_H_
