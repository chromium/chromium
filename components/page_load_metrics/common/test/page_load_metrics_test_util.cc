// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"

#include "base/byte_count.h"
#include "components/page_load_metrics/common/page_load_metrics_util.h"

using page_load_metrics::OptionalMin;

void PopulateRequiredTimingFields(
    page_load_metrics::mojom::PageLoadTiming* inout_timing) {
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
      !inout_timing->paint_timing->first_eligible_to_paint) {
    inout_timing->paint_timing->first_eligible_to_paint =
        inout_timing->paint_timing->first_paint;
  }
  if (inout_timing->document_timing->load_event_start &&
      !inout_timing->document_timing->dom_content_loaded_event_start) {
    inout_timing->document_timing->dom_content_loaded_event_start =
        inout_timing->document_timing->load_event_start;
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
    if (!inout_timing->parse_timing->parse_blocked_on_script_load_duration) {
      inout_timing->parse_timing->parse_blocked_on_script_load_duration =
          base::TimeDelta();
    }
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

// Sets the experimental LCP values to be equal to the non-experimental
// counterparts.
void PopulateExperimentalLCP(page_load_metrics::mojom::PaintTimingPtr& timing) {
  timing->experimental_largest_contentful_paint =
      timing->largest_contentful_paint->Clone();
}

page_load_metrics::mojom::ResourceDataUpdatePtr CreateResource(
    bool was_cached,
    base::ByteCount delta_bytes,
    base::ByteCount encoded_body_length,
    base::ByteCount decoded_body_length,
    bool is_complete) {
  auto resource_data_update =
      page_load_metrics::mojom::ResourceDataUpdate::New();
  resource_data_update->cache_type =
      was_cached ? page_load_metrics::mojom::CacheType::kHttp
                 : page_load_metrics::mojom::CacheType::kNotCached;
  resource_data_update->delta_bytes = delta_bytes;
  resource_data_update->received_data_length = delta_bytes;
  resource_data_update->encoded_body_length = encoded_body_length;
  resource_data_update->decoded_body_length = decoded_body_length;
  resource_data_update->is_complete = is_complete;
  return resource_data_update;
}

std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>
GetSampleResourceDataUpdateForTesting(base::ByteCount resource_size) {
  // Prepare 3 resources of varying configurations.
  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // Cached resource.
  resources.push_back(CreateResource(/*was_cached=*/true,
                                     /*delta_bytes=*/base::ByteCount(0),
                                     /*encoded_body_length=*/resource_size,
                                     /*decoded_body_length=*/resource_size,
                                     /*is_complete=*/true));
  // Uncached resource.
  resources.push_back(CreateResource(
      /*was_cached=*/false, /*delta_bytes=*/resource_size,
      /*encoded_body_length=*/resource_size,
      /*decoded_body_length=*/resource_size, /*is_complete=*/true));
  // Uncached, unfinished, resource.
  resources.push_back(CreateResource(
      /*was_cached=*/false, /*delta_bytes=*/resource_size,
      /*encoded_body_length=*/base::ByteCount(0),
      /*decoded_body_length=*/base::ByteCount(0),
      /*is_complete=*/false));
  return resources;
}
