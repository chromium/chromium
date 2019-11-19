// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_RESOURCE_DATA_USE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_RESOURCE_DATA_USE_H_

#include "base/macros.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/origin.h"

class GURL;

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace page_load_metrics {

// PageResourceDataUse contains the data use information of one resource. Data
// use is updated when resource size updates are called.
class PageResourceDataUse {
 public:
  PageResourceDataUse();
  PageResourceDataUse(const PageResourceDataUse& other);
  ~PageResourceDataUse();

  void DidStartResponse(const url::Origin& origin_of_final_response_url,
                        int resource_id,
                        const network::mojom::URLResponseHead& response_head,
                        content::ResourceType resource_type,
                        content::PreviewsState previews_state);

  // Updates received bytes.
  void DidReceiveTransferSizeUpdate(int received_data_length);

  // Updates received bytes from encoded length.
  void DidCompleteResponse(const network::URLLoaderCompletionStatus& status);

  // Flags the resource as canceled.
  void DidCancelResponse();

  // Called when this resource was loaded from the memory cache. Resources
  // loaded from the memory cache only receive a single update.
  void DidLoadFromMemoryCache(const GURL& response_url,
                              int request_id,
                              int64_t encoded_body_length,
                              const std::string& mime_type);

  // Checks if the resource has completed loading or if the response was
  // canceled.
  bool IsFinishedLoading();

  int resource_id() const { return resource_id_; }

  void SetReportedAsAdResource(bool reported_as_ad_resource);
  void SetIsMainFrameResource(bool is_main_frame_resource);
  void SetCompletedBeforeFCP(bool completed_before_fcp);

  // Creates a ResourceDataUpdate mojo for this resource. This page resource
  // contains information since the last time update. Should be called at most
  // once once per timing update.
  mojom::ResourceDataUpdatePtr GetResourceDataUpdate();

 private:
  // Calculates the difference between |total_received_bytes_| and
  // |last_update_bytes_|, returns it, and updates |last_update_bytes_|.
  int CalculateNewlyReceivedBytes();

  int resource_id_;

  // Compression ratio estimated from the response headers if data saver was
  // used.
  double data_reduction_proxy_compression_ratio_estimate_;

  uint64_t total_received_bytes_ = 0;
  uint64_t last_update_bytes_ = 0;
  uint64_t encoded_body_length_ = 0;

  bool is_complete_;
  bool is_canceled_;
  bool reported_as_ad_resource_;
  bool is_main_frame_resource_;
  bool is_secure_scheme_;
  bool proxy_used_;
  bool is_primary_frame_resource_;
  bool completed_before_fcp_;

  mojom::CacheType cache_type_;

  url::Origin origin_;

  std::string mime_type_;

  DISALLOW_ASSIGN(PageResourceDataUse);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_RESOURCE_DATA_USE_H_
