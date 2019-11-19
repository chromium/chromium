// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"

#include "components/page_load_metrics/common/page_load_metrics_util.h"

using page_load_metrics::OptionalMin;

void PopulateRequiredTimingFields(
    page_load_metrics::mojom::PageLoadTiming* inout_timing) {
  if (inout_timing->interactive_timing->interactive_detection &&
      !inout_timing->interactive_timing->interactive) {
    inout_timing->interactive_timing->interactive =
        inout_timing->interactive_timing->interactive_detection;
  }
  if (inout_timing->interactive_timing->interactive &&
      !inout_timing->paint_timing->first_meaningful_paint) {
    inout_timing->paint_timing->first_meaningful_paint =
        inout_timing->interactive_timing->interactive;
  }
  if (inout_timing->paint_timing->first_meaningful_paint &&
      !inout_timing->paint_timing->first_contentful_paint) {
    inout_timing->paint_timing->first_contentful_paint =
        inout_timing->paint_timing->first_meaningful_paint;
  }
  if ((inout_timing->paint_timing->first_image_paint ||
       inout_timing->paint_timing->first_contentful_paint) &&
      !inout_timing->paint_timing->first_paint) {
    inout_timing->paint_timing->first_paint =
        OptionalMin(inout_timing->paint_timing->first_image_paint,
                    inout_timing->paint_timing->first_contentful_paint);
  }
  if (inout_timing->paint_timing->first_paint &&
      !inout_timing->document_timing->first_layout) {
    inout_timing->document_timing->first_layout =
        inout_timing->paint_timing->first_paint;
  }
  if (inout_timing->document_timing->load_event_start &&
      !inout_timing->document_timing->dom_content_loaded_event_start) {
    inout_timing->document_timing->dom_content_loaded_event_start =
        inout_timing->document_timing->load_event_start;
  }
  if (inout_timing->document_timing->first_layout &&
      !inout_timing->parse_timing->parse_start) {
    inout_timing->parse_timing->parse_start =
        inout_timing->document_timing->first_layout;
  }
  if (inout_timing->document_timing->dom_content_loaded_event_start &&
      !inout_timing->parse_timing->parse_stop) {
    inout_timing->parse_timing->parse_stop =
        inout_timing->document_timing->dom_content_loaded_event_start;
  }
  if (inout_timing->parse_timing->parse_stop &&
      !inout_timing->parse_timing->parse_start) {
    inout_timing->parse_timing->parse_start =
        inout_timing->parse_timing->parse_stop;
  }
  if (inout_timing->parse_timing->parse_start &&
      !inout_timing->response_start) {
    inout_timing->response_start = inout_timing->parse_timing->parse_start;
  }
  if (inout_timing->parse_timing->parse_start) {
    if (!inout_timing->parse_timing->parse_blocked_on_script_load_duration)
      inout_timing->parse_timing->parse_blocked_on_script_load_duration =
          base::TimeDelta();
    if (!inout_timing->parse_timing
             ->parse_blocked_on_script_execution_duration) {
      inout_timing->parse_timing->parse_blocked_on_script_execution_duration =
          base::TimeDelta();
    }
    if (!inout_timing->parse_timing
             ->parse_blocked_on_script_load_from_document_write_duration) {
      inout_timing->parse_timing
          ->parse_blocked_on_script_load_from_document_write_duration =
          base::TimeDelta();
    }
    if (!inout_timing->parse_timing
             ->parse_blocked_on_script_execution_from_document_write_duration) {
      inout_timing->parse_timing
          ->parse_blocked_on_script_execution_from_document_write_duration =
          base::TimeDelta();
    }
  }
}

page_load_metrics::mojom::ResourceDataUpdatePtr CreateResource(
    bool was_cached,
    int64_t delta_bytes,
    int64_t encoded_body_length,
    bool is_complete) {
  auto resource_data_update =
      page_load_metrics::mojom::ResourceDataUpdate::New();
  resource_data_update->cache_type =
      was_cached ? page_load_metrics::mojom::CacheType::kHttp
                 : page_load_metrics::mojom::CacheType::kNotCached;
  resource_data_update->delta_bytes = delta_bytes;
  resource_data_update->received_data_length = delta_bytes;
  resource_data_update->encoded_body_length = encoded_body_length;
  resource_data_update->is_complete = is_complete;
  return resource_data_update;
}

std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>
GetSampleResourceDataUpdateForTesting(int64_t resource_size) {
  // Prepare 3 resources of varying configurations.
  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // Cached resource.
  resources.push_back(CreateResource(true /* was_cached */, 0 /* delta_bytes */,
                                     resource_size /* encoded_body_length */,
                                     true /* is_complete */));
  // Uncached resource.
  resources.push_back(CreateResource(
      false /* was_cached */, resource_size /* delta_bytes */,
      resource_size /* encoded_body_length */, true /* is_complete */));
  // Uncached, unfinished, resource.
  resources.push_back(
      CreateResource(false /* was_cached */, resource_size /* delta_bytes */,
                     0 /* encoded_body_length */, false /* is_complete */));
  return resources;
}
