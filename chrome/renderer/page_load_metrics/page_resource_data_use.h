// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_RESOURCE_DATA_USE_H_
#define CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_RESOURCE_DATA_USE_H_

#include "base/macros.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"

namespace network {
struct ResourceResponseHead;
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

  void DidStartResponse(int resource_id,
                        const network::ResourceResponseHead& response_head);

  // Updates received bytes.
  void DidReceiveTransferSizeUpdate(int received_data_length);

  // Updates received bytes from encoded length, returns whether it was updated.
  bool DidCompleteResponse(const network::URLLoaderCompletionStatus& status);

  // Flags the resource as canceled.
  void DidCancelResponse();

  // Checks if the resource has completed loading or if the response was
  // cancelled.
  bool IsFinishedLoading();

  int resource_id() const { return resource_id_; }

  void SetReportedAsAdResource(bool reported_as_ad_resource);
  void SetIsMainFrameResource(bool is_main_frame_resource);

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

  uint64_t total_received_bytes_;
  uint64_t last_update_bytes_;
  uint64_t encoded_body_length_ = 0;

  bool is_complete_;
  bool is_canceled_;
  bool reported_as_ad_resource_;
  bool is_main_frame_resource_;
  bool was_fetched_via_cache_;

  std::string mime_type_;

  DISALLOW_ASSIGN(PageResourceDataUse);
};

}  // namespace page_load_metrics

#endif  // CHROME_RENDERER_PAGE_LOAD_METRICS_PAGE_RESOURCE_DATA_USE_H_
