// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_MOJOM_TRAITS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_MOJOM_TRAITS_H_

#include "components/page_load_metrics/common/page_load_metrics.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"

namespace mojo {

template <>
class StructTraits<page_load_metrics::mojom::SubresourceLoadMetricsDataView,
                   blink::SubresourceLoadMetrics> {
 public:
  static uint32_t number_of_subresources_loaded(
      const blink::SubresourceLoadMetrics& d) {
    return d.number_of_subresources_loaded;
  }
  static uint32_t number_of_subresource_loads_handled_by_service_worker(
      const blink::SubresourceLoadMetrics& d) {
    return d.number_of_subresource_loads_handled_by_service_worker;
  }
  static bool pervasive_payload_requested(
      const blink::SubresourceLoadMetrics& d) {
    return d.pervasive_payload_requested;
  }
  static int64_t pervasive_bytes_fetched(
      const blink::SubresourceLoadMetrics& d) {
    return d.pervasive_bytes_fetched;
  }
  static int64_t total_bytes_fetched(const blink::SubresourceLoadMetrics& d) {
    return d.total_bytes_fetched;
  }
  static bool Read(
      page_load_metrics::mojom::SubresourceLoadMetricsDataView data,
      blink::SubresourceLoadMetrics* out);
};

}  // namespace mojo

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_MOJOM_TRAITS_H_
