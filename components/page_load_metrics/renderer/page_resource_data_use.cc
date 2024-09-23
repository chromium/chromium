// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_resource_data_use.h"

#include "net/base/proxy_chain.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace page_load_metrics {

PageResourceDataUse::PageResourceDataUse(int resource_id)
    : resource_id_(resource_id) {}

PageResourceDataUse::PageResourceDataUse(const PageResourceDataUse& other) =
    default;
PageResourceDataUse::~PageResourceDataUse() = default;

void PageResourceDataUse::DidStartResponse(
    const url::SchemeHostPort& final_response_url,
    int resource_id,
    const network::mojom::URLResponseHead& response_head,
    network::mojom::RequestDestination request_destination,
    bool is_ad_resource) {
  if (resource_id_ != kUnknownResourceId) {
    CHECK_EQ(resource_id_, resource_id);
  }
  resource_id_ = resource_id;

  proxy_used_ = !response_head.proxy_chain.is_direct();
  mime_type_ = response_head.mime_type;
  if (response_head.was_fetched_via_cache)
    cache_type_ = mojom::CacheType::kHttp;
  is_secure_scheme_ = GURL::SchemeIsCryptographic(final_response_url.scheme());
  is_primary_frame_resource_ =
      blink::IsRequestDestinationFrame(request_destination);
  reported_as_ad_resource_ = is_ad_resource;
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
  decoded_body_length_ = status.decoded_body_length;
  int64_t delta_bytes = status.encoded_data_length - total_received_bytes_;
  if (delta_bytes > 0) {
    total_received_bytes_ += delta_bytes;
  }
}

void PageResourceDataUse::DidCancelResponse() {
  is_canceled_ = true;
}

void PageResourceDataUse::DidLoadFromMemoryCache(const GURL& response_url,
                                                 int64_t encoded_body_length,
                                                 const std::string& mime_type) {
  // Resource id was set in the constructor.
  CHECK_NE(resource_id_, kUnknownResourceId);

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

void PageResourceDataUse::SetIsMainFrameResource(bool is_main_frame_resource) {
  is_main_frame_resource_ = is_main_frame_resource;
}

int64_t PageResourceDataUse::CalculateNewlyReceivedBytes() {
  int64_t newly_received_bytes = total_received_bytes_ - last_update_bytes_;
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
  resource_data_update->reported_as_ad_resource = reported_as_ad_resource_;
  resource_data_update->is_main_frame_resource = is_main_frame_resource_;
  resource_data_update->mime_type = mime_type_;
  resource_data_update->encoded_body_length = encoded_body_length_;
  resource_data_update->decoded_body_length = decoded_body_length_;
  resource_data_update->cache_type = cache_type_;
  resource_data_update->is_secure_scheme = is_secure_scheme_;
  resource_data_update->proxy_used = proxy_used_;
  resource_data_update->is_primary_frame_resource = is_primary_frame_resource_;
  return resource_data_update;
}
}  // namespace page_load_metrics
