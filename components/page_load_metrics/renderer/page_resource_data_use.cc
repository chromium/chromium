// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_resource_data_use.h"

#include "base/stl_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace page_load_metrics {

namespace {

// Returns true when the image is a placeholder for lazy load.
bool IsPartialImageRequest(content::ResourceType resource_type,
                           content::PreviewsState previews_state) {
  if (resource_type != content::ResourceType::kImage)
    return false;
  return previews_state & content::PreviewsTypes::LAZY_IMAGE_LOAD_DEFERRED;
}

// Returns true if this resource was previously fetched as a placeholder.
bool IsImageAutoReload(content::ResourceType resource_type,
                       content::PreviewsState previews_state) {
  if (resource_type != content::ResourceType::kImage)
    return false;
  return previews_state & content::PreviewsTypes::LAZY_IMAGE_AUTO_RELOAD;
}

// Returns the ratio of original data size (without applying interventions) to
// actual data use for a placeholder.
double EstimatePartialImageRequestSavings(
    const network::mojom::URLResponseHead& response_head) {
  if (!response_head.headers)
    return 1.0;

  if (response_head.headers->GetContentLength() <= 0)
    return 1.0;

  int64_t first, last, original_length;
  if (!response_head.headers->GetContentRangeFor206(&first, &last,
                                                    &original_length)) {
    return 1.0;
  }

  if (original_length < response_head.headers->GetContentLength())
    return 1.0;

  return original_length / response_head.headers->GetContentLength();
}

// Returns a ratio of original data use to actual data use while factoring in
// that this request was previously fetched as a placeholder, and therefore
// recorded savings earlier.
double EstimateAutoReloadImageRequestSavings(
    const network::mojom::URLResponseHead& response_head) {
  static const double kPlageholderContentInCache = 2048;

  // Count the new network usage. For a reloaded placeholder image, 2KB will be
  // in cache.
  return kPlageholderContentInCache / response_head.headers->GetContentLength();
}

}  // namespace

PageResourceDataUse::PageResourceDataUse()
    : resource_id_(-1),
      data_reduction_proxy_compression_ratio_estimate_(1.0),
      total_received_bytes_(0),
      last_update_bytes_(0),
      is_complete_(false),
      is_canceled_(false),
      reported_as_ad_resource_(false),
      is_main_frame_resource_(false),
      is_secure_scheme_(false),
      proxy_used_(false),
      is_primary_frame_resource_(false),
      completed_before_fcp_(false),
      cache_type_(mojom::CacheType::kNotCached) {}

PageResourceDataUse::PageResourceDataUse(const PageResourceDataUse& other) =
    default;
PageResourceDataUse::~PageResourceDataUse() = default;

void PageResourceDataUse::DidStartResponse(
    const url::Origin& origin_of_final_response_url,
    int resource_id,
    const network::mojom::URLResponseHead& response_head,
    content::ResourceType resource_type,
    content::PreviewsState previews_state) {
  resource_id_ = resource_id;

  if (IsPartialImageRequest(resource_type, previews_state)) {
    data_reduction_proxy_compression_ratio_estimate_ =
        EstimatePartialImageRequestSavings(response_head);
  } else if (IsImageAutoReload(resource_type, previews_state)) {
    data_reduction_proxy_compression_ratio_estimate_ =
        EstimateAutoReloadImageRequestSavings(response_head);
  } else {
    data_reduction_proxy_compression_ratio_estimate_ =
        data_reduction_proxy::EstimateCompressionRatioFromHeaders(
            &response_head);
  }

  proxy_used_ = !response_head.proxy_server.is_direct();
  mime_type_ = response_head.mime_type;
  if (response_head.was_fetched_via_cache)
    cache_type_ = mojom::CacheType::kHttp;
  is_primary_frame_resource_ =
      resource_type == content::ResourceType::kMainFrame ||
      resource_type == content::ResourceType::kSubFrame;
  origin_ = origin_of_final_response_url;
  is_secure_scheme_ = GURL::SchemeIsCryptographic(origin_.scheme());
}

void PageResourceDataUse::DidReceiveTransferSizeUpdate(
    int received_data_length) {
  total_received_bytes_ += received_data_length;
}

void PageResourceDataUse::DidCompleteResponse(
    const network::URLLoaderCompletionStatus& status) {
  // Report the difference in received bytes.
  is_complete_ = true;
  encoded_body_length_ = status.encoded_body_length;
  int64_t delta_bytes = status.encoded_data_length - total_received_bytes_;
  if (delta_bytes > 0) {
    total_received_bytes_ += delta_bytes;
  }
}

void PageResourceDataUse::DidCancelResponse() {
  is_canceled_ = true;
}

void PageResourceDataUse::DidLoadFromMemoryCache(const GURL& response_url,
                                                 int request_id,
                                                 int64_t encoded_body_length,
                                                 const std::string& mime_type) {
  origin_ = url::Origin::Create(response_url);
  resource_id_ = request_id;
  mime_type_ = mime_type;
  is_secure_scheme_ = response_url.SchemeIsCryptographic();
  cache_type_ = mojom::CacheType::kMemory;

  // Resources from the memory cache cannot be a primary frame resource.
  is_primary_frame_resource_ = false;

  is_complete_ = true;
  encoded_body_length_ = encoded_body_length;
}

bool PageResourceDataUse::IsFinishedLoading() {
  return is_complete_ || is_canceled_;
}

void PageResourceDataUse::SetReportedAsAdResource(
    bool reported_as_ad_resource) {
  reported_as_ad_resource_ = reported_as_ad_resource;
}

void PageResourceDataUse::SetIsMainFrameResource(bool is_main_frame_resource) {
  is_main_frame_resource_ = is_main_frame_resource;
}

void PageResourceDataUse::SetCompletedBeforeFCP(bool completed_before_fcp) {
  DCHECK(completed_before_fcp);
  completed_before_fcp_ = completed_before_fcp;
}

int PageResourceDataUse::CalculateNewlyReceivedBytes() {
  int newly_received_bytes = total_received_bytes_ - last_update_bytes_;
  last_update_bytes_ = total_received_bytes_;
  DCHECK_GE(newly_received_bytes, 0);
  return newly_received_bytes;
}

mojom::ResourceDataUpdatePtr PageResourceDataUse::GetResourceDataUpdate() {
  DCHECK(cache_type_ == mojom::CacheType::kMemory ? is_complete_ : true);
  mojom::ResourceDataUpdatePtr resource_data_update =
      mojom::ResourceDataUpdate::New();
  resource_data_update->request_id = resource_id();
  resource_data_update->received_data_length = total_received_bytes_;
  resource_data_update->delta_bytes = CalculateNewlyReceivedBytes();
  resource_data_update->is_complete = is_complete_;
  resource_data_update->data_reduction_proxy_compression_ratio_estimate =
      data_reduction_proxy_compression_ratio_estimate_;
  resource_data_update->reported_as_ad_resource = reported_as_ad_resource_;
  resource_data_update->is_main_frame_resource = is_main_frame_resource_;
  resource_data_update->mime_type = mime_type_;
  resource_data_update->encoded_body_length = encoded_body_length_;
  resource_data_update->cache_type = cache_type_;
  resource_data_update->is_secure_scheme = is_secure_scheme_;
  resource_data_update->proxy_used = proxy_used_;
  resource_data_update->is_primary_frame_resource = is_primary_frame_resource_;
  resource_data_update->origin = origin_;
  resource_data_update->completed_before_fcp = completed_before_fcp_;
  return resource_data_update;
}
}  // namespace page_load_metrics
