// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_metrics_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<page_load_metrics::mojom::SubresourceLoadMetricsDataView,
                  blink::SubresourceLoadMetrics>::
    Read(page_load_metrics::mojom::SubresourceLoadMetricsDataView data,
         blink::SubresourceLoadMetrics* out) {
  out->number_of_subresources_loaded = data.number_of_subresources_loaded();
  out->number_of_subresource_loads_handled_by_service_worker =
      data.number_of_subresource_loads_handled_by_service_worker();
  return data.ReadServiceWorkerSubresourceLoadMetrics(
      &out->service_worker_subresource_load_metrics);
}

// static
bool StructTraits<
    page_load_metrics::mojom::ServiceWorkerSubresourceLoadMetricsDataView,
    blink::ServiceWorkerSubresourceLoadMetrics>::
    Read(page_load_metrics::mojom::ServiceWorkerSubresourceLoadMetricsDataView
             data,
         blink::ServiceWorkerSubresourceLoadMetrics* out) {
  out->image_handled = data.image_handled();
  out->image_fallback = data.image_fallback();
  out->css_handled = data.css_handled();
  out->css_fallback = data.css_fallback();
  out->script_handled = data.script_handled();
  out->script_fallback = data.script_fallback();
  out->font_handled = data.font_handled();
  out->font_fallback = data.font_fallback();
  out->raw_handled = data.raw_handled();
  out->raw_fallback = data.raw_fallback();
  out->svg_handled = data.svg_handled();
  out->svg_fallback = data.svg_fallback();
  out->xsl_handled = data.xsl_handled();
  out->xsl_fallback = data.xsl_fallback();
  out->link_prefetch_handled = data.link_prefetch_handled();
  out->link_prefetch_fallback = data.link_prefetch_fallback();
  out->text_track_handled = data.text_track_handled();
  out->text_track_fallback = data.text_track_fallback();
  out->audio_handled = data.audio_handled();
  out->audio_fallback = data.audio_fallback();
  out->video_handled = data.video_handled();
  out->video_fallback = data.video_fallback();
  out->manifest_handled = data.manifest_handled();
  out->manifest_fallback = data.manifest_fallback();
  out->speculation_rules_handled = data.speculation_rules_handled();
  out->speculation_rules_fallback = data.speculation_rules_fallback();
  out->mock_handled = data.mock_handled();
  out->mock_fallback = data.mock_fallback();
  out->dictionary_handled = data.dictionary_handled();
  out->dictionary_fallback = data.dictionary_fallback();
  out->matched_cache_router_source_count =
      data.matched_cache_router_source_count();
  out->matched_fetch_event_router_source_count =
      data.matched_fetch_event_router_source_count();
  out->matched_network_router_source_count =
      data.matched_network_router_source_count();
  out->matched_race_network_and_fetch_router_source_count =
      data.matched_race_network_and_fetch_router_source_count();
  if (!data.ReadTotalRouterEvaluationTimeForSubresources(
          &out->total_router_evaluation_time_for_subresources)) {
    return false;
  }
  if (!data.ReadTotalCacheLookupTimeForSubresources(
          &out->total_cache_lookup_time_for_subresources)) {
    return false;
  }
  return true;
}

}  // namespace mojo
