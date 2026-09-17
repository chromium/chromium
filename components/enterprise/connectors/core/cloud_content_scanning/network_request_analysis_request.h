// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_ANALYSIS_REQUEST_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_ANALYSIS_REQUEST_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace enterprise_connectors {

// A `BinaryUploadRequest` implementation that gets the data to scan from a
// `network::ResourceRequestBody` corresponding to a network request.
class NetworkRequestAnalysisRequest : public BinaryUploadRequest {
 public:
  NetworkRequestAnalysisRequest(
      CloudAnalysisSettings settings,
      scoped_refptr<network::ResourceRequestBody> request_body,
      BinaryUploadRequest::ContentAnalysisCallback callback,
      BrowserPolicyConnectorGetter policy_connector_getter);
  ~NetworkRequestAnalysisRequest() override;

  NetworkRequestAnalysisRequest(const NetworkRequestAnalysisRequest&) = delete;
  NetworkRequestAnalysisRequest& operator=(
      const NetworkRequestAnalysisRequest&) = delete;

  // BinaryUploadRequest:
  void GetRequestData(DataCallback callback) override;

 private:
  class DataPipeSizeGetter;

  void OnAllElementSizesComputed(
      uint64_t sync_bytes_sum,
      std::vector<std::optional<uint64_t>> async_sizes);
  // Caches `size` along with the result implied by it, then runs any pending
  // callbacks.
  void CacheResultAndData(uint64_t size);

  // Same as above, but uses `result` instead of deriving it from `size`. Used
  // when the body can't be scanned for a reason the size alone doesn't convey.
  void CacheResultAndData(uint64_t size, ScanRequestUploadResult result);

  scoped_refptr<network::ResourceRequestBody> request_body_;

  Data data_;
  std::optional<ScanRequestUploadResult> result_;

  std::vector<DataCallback> pending_callbacks_;
  std::vector<std::unique_ptr<DataPipeSizeGetter>> data_pipe_getters_;

  base::WeakPtrFactory<NetworkRequestAnalysisRequest> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_ANALYSIS_REQUEST_H_
