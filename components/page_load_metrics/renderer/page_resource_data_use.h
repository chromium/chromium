// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_RESOURCE_DATA_USE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_RESOURCE_DATA_USE_H_

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

class GURL;

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace url {
class SchemeHostPort;
}  // namespace url

namespace page_load_metrics {

// PageResourceDataUse contains the data use information of one resource. Data
// use is updated when resource size updates are called.
class PageResourceDataUse {
 public:
  // Placeholder for a resource id that isn't known yet.
  static constexpr int kUnknownResourceId = -1;

  explicit PageResourceDataUse(int resource_id);
  PageResourceDataUse(const PageResourceDataUse& other);

  PageResourceDataUse& operator=(const PageResourceDataUse&) = delete;

  ~PageResourceDataUse();

  void DidStartResponse(const url::SchemeHostPort& final_response_url,
                        int resource_id,
                        const network::mojom::URLResponseHead& response_head,
                        network::mojom::RequestDestination request_destination,
                        bool is_ad_resource);

  // Updates received bytes.
  void DidReceiveTransferSizeUpdate(int received_data_length);

  // Updates received bytes information and decoded body length using the final
  // state of the resource load.
  void DidCompleteResponse(const network::URLLoaderCompletionStatus& status);

  // Flags the resource as canceled.
  void DidCancelResponse();

  // Called when this resource was loaded from the memory cache. Resources
  // loaded from the memory cache only receive a single update.
  void DidLoadFromMemoryCache(const GURL& response_url,
                              int64_t encoded_body_length,
                              const std::string& mime_type);

  // Checks if the resource has completed loading or if the response was
  // canceled.
  bool IsFinishedLoading();

  int resource_id() const { return resource_id_; }

  void SetIsMainFrameResource(bool is_main_frame_resource);

  // Creates a ResourceDataUpdate mojo for this resource. This page resource
  // contains information since the last time update. Should be called at most
  // once once per timing update.
  mojom::ResourceDataUpdatePtr GetResourceDataUpdate();

 private:
  // Calculates the difference between |total_received_bytes_| and
  // |last_update_bytes_|, returns it, and updates |last_update_bytes_|.
  int64_t CalculateNewlyReceivedBytes();

  int resource_id_ = kUnknownResourceId;

  int64_t total_received_bytes_ = 0;
  int64_t last_update_bytes_ = 0;
  int64_t encoded_body_length_ = 0;
  int64_t decoded_body_length_ = 0;

  bool is_complete_ = false;
  bool is_canceled_ = false;
  bool reported_as_ad_resource_ = false;
  bool is_main_frame_resource_ = false;
  bool is_secure_scheme_ = false;
  bool proxy_used_ = false;
  bool is_primary_frame_resource_ = false;

  mojom::CacheType cache_type_ = mojom::CacheType::kNotCached;

  std::string mime_type_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_RESOURCE_DATA_USE_H_
