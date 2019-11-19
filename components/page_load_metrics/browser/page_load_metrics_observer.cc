// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

#include <utility>

namespace page_load_metrics {

ExtraRequestCompleteInfo::ExtraRequestCompleteInfo(
    const url::Origin& origin_of_final_url,
    const net::IPEndPoint& remote_endpoint,
    int frame_tree_node_id,
    bool was_cached,
    int64_t raw_body_bytes,
    int64_t original_network_content_length,
    std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
        data_reduction_proxy_data,
    content::ResourceType detected_resource_type,
    int net_error,
    std::unique_ptr<net::LoadTimingInfo> load_timing_info)
    : origin_of_final_url(origin_of_final_url),
      remote_endpoint(remote_endpoint),
      frame_tree_node_id(frame_tree_node_id),
      was_cached(was_cached),
      raw_body_bytes(raw_body_bytes),
      original_network_content_length(original_network_content_length),
      data_reduction_proxy_data(std::move(data_reduction_proxy_data)),
      resource_type(detected_resource_type),
      net_error(net_error),
      load_timing_info(std::move(load_timing_info)) {}

ExtraRequestCompleteInfo::ExtraRequestCompleteInfo(
    const ExtraRequestCompleteInfo& other)
    : origin_of_final_url(other.origin_of_final_url),
      remote_endpoint(other.remote_endpoint),
      frame_tree_node_id(other.frame_tree_node_id),
      was_cached(other.was_cached),
      raw_body_bytes(other.raw_body_bytes),
      original_network_content_length(other.original_network_content_length),
      data_reduction_proxy_data(
          other.data_reduction_proxy_data == nullptr
              ? nullptr
              : other.data_reduction_proxy_data->DeepCopy()),
      resource_type(other.resource_type),
      net_error(other.net_error),
      load_timing_info(other.load_timing_info == nullptr
                           ? nullptr
                           : std::make_unique<net::LoadTimingInfo>(
                                 *other.load_timing_info)) {}

ExtraRequestCompleteInfo::~ExtraRequestCompleteInfo() {}

FailedProvisionalLoadInfo::FailedProvisionalLoadInfo(base::TimeDelta interval,
                                                     net::Error error)
    : time_to_failed_provisional_load(interval), error(error) {}

FailedProvisionalLoadInfo::~FailedProvisionalLoadInfo() {}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnHidden(
    const mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy PageLoadMetricsObserver::OnShown() {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

PageLoadMetricsObserver::ObservePolicy
PageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  return IsStandardWebPageMimeType(mime_type) ? CONTINUE_OBSERVING
                                              : STOP_OBSERVING;
}

// static
bool PageLoadMetricsObserver::IsStandardWebPageMimeType(
    const std::string& mime_type) {
  return mime_type == "text/html" || mime_type == "application/xhtml+xml";
}

// static
bool PageLoadMetricsObserver::AssignTimeAndSizeForLargestContentfulPaint(
    const page_load_metrics::mojom::PaintTimingPtr& paint_timing,
    base::Optional<base::TimeDelta>* largest_content_paint_time,
    uint64_t* largest_content_paint_size,
    LargestContentType* largest_content_type) {
  base::Optional<base::TimeDelta>& text_time = paint_timing->largest_text_paint;
  base::Optional<base::TimeDelta>& image_time =
      paint_timing->largest_image_paint;
  uint64_t text_size = paint_timing->largest_text_paint_size;
  uint64_t image_size = paint_timing->largest_image_paint_size;

  // Size being 0 means the paint time is not recorded.
  if (!text_size && !image_size)
    return false;

  if ((text_size > image_size) ||
      (text_size == image_size && text_time < image_time)) {
    *largest_content_paint_time = text_time;
    *largest_content_paint_size = text_size;
    *largest_content_type = LargestContentType::kText;
  } else {
    *largest_content_paint_time = image_time;
    *largest_content_paint_size = image_size;
    *largest_content_type = LargestContentType::kImage;
  }
  return true;
}

const PageLoadMetricsObserverDelegate& PageLoadMetricsObserver::GetDelegate()
    const {
  // The delegate must exist and outlive the page load metrics observer.
  DCHECK(delegate_);
  return *delegate_;
}

void PageLoadMetricsObserver::SetDelegate(
    PageLoadMetricsObserverDelegate* delegate) {
  delegate_ = delegate;
}

}  // namespace page_load_metrics
