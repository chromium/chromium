// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_metrics_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<page_load_metrics::mojom::SubresourceLoadMetricsDataView,
                  blink::SubresourceLoadMetrics>::
    Read(page_load_metrics::mojom::SubresourceLoadMetricsDataView data,
         blink::SubresourceLoadMetrics* out) {
  out->number_of_subresources_loaded = data.number_of_subresources_loaded();
  out->number_of_subresource_loads_handled_by_service_worker =
      data.number_of_subresource_loads_handled_by_service_worker();
  out->pervasive_payload_requested = data.pervasive_payload_requested();
  out->total_bytes_fetched = data.total_bytes_fetched();
  out->pervasive_bytes_fetched = data.pervasive_bytes_fetched();
  return true;
}

}  // namespace mojo
