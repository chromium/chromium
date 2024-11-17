// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_MOJOM_TRAITS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_MOJOM_TRAITS_H_

#include <cstdint>

#include "base/time/time.h"
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
  static std::optional<blink::ServiceWorkerSubresourceLoadMetrics>
  service_worker_subresource_load_metrics(
      const blink::SubresourceLoadMetrics& d) {
    return d.service_worker_subresource_load_metrics;
  }
  static bool Read(
      page_load_metrics::mojom::SubresourceLoadMetricsDataView data,
      blink::SubresourceLoadMetrics* out);
};

template <>
class StructTraits<
    page_load_metrics::mojom::ServiceWorkerSubresourceLoadMetricsDataView,
    blink::ServiceWorkerSubresourceLoadMetrics> {
 public:
  static bool image_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.image_handled;
  }
  static bool image_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.image_fallback;
  }
  static bool css_handled(const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.css_handled;
  }
  static bool css_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.css_fallback;
  }
  static bool script_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.script_handled;
  }
  static bool script_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.script_fallback;
  }
  static bool font_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.font_handled;
  }
  static bool font_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.font_fallback;
  }
  static bool raw_handled(const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.raw_handled;
  }
  static bool raw_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.raw_fallback;
  }
  static bool svg_handled(const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.svg_handled;
  }
  static bool svg_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.svg_fallback;
  }
  static bool xsl_handled(const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.xsl_handled;
  }
  static bool xsl_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.xsl_fallback;
  }
  static bool link_prefetch_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.link_prefetch_handled;
  }
  static bool link_prefetch_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.link_prefetch_fallback;
  }
  static bool text_track_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.text_track_handled;
  }
  static bool text_track_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.text_track_fallback;
  }
  static bool audio_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.audio_handled;
  }
  static bool audio_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.audio_fallback;
  }
  static bool video_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.video_handled;
  }
  static bool video_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.video_fallback;
  }
  static bool manifest_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.manifest_handled;
  }
  static bool manifest_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.manifest_fallback;
  }
  static bool speculation_rules_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.speculation_rules_handled;
  }
  static bool speculation_rules_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.speculation_rules_fallback;
  }
  static bool mock_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.mock_handled;
  }
  static bool mock_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.mock_fallback;
  }

  static bool dictionary_handled(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.dictionary_handled;
  }
  static bool dictionary_fallback(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.dictionary_fallback;
  }

  static uint32_t matched_cache_router_source_count(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.matched_cache_router_source_count;
  }

  static uint32_t matched_fetch_event_router_source_count(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.matched_fetch_event_router_source_count;
  }

  static uint32_t matched_network_router_source_count(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.matched_network_router_source_count;
  }

  static uint32_t matched_race_network_and_fetch_router_source_count(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.matched_race_network_and_fetch_router_source_count;
  }

  static base::TimeDelta total_router_evaluation_time_for_subresources(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.total_router_evaluation_time_for_subresources;
  }

  static base::TimeDelta total_cache_lookup_time_for_subresources(
      const blink::ServiceWorkerSubresourceLoadMetrics& d) {
    return d.total_cache_lookup_time_for_subresources;
  }

  static bool Read(
      page_load_metrics::mojom::ServiceWorkerSubresourceLoadMetricsDataView
          data,
      blink::ServiceWorkerSubresourceLoadMetrics* out);
};

}  // namespace mojo

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_METRICS_MOJOM_TRAITS_H_
