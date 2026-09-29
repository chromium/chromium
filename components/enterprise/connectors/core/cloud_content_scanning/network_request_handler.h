// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_HANDLER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_HANDLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/request_handler_base.h"
#include "components/enterprise/connectors/core/common.h"
#include "url/gurl.h"

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace enterprise_connectors {

class BinaryUploadService;
class ContentAnalysisInfoBase;
class NetworkRequestAnalysisRequest;
class ReportingEventRouter;

// Handles the scanning and reporting of the body of a single network request
// for the "OnNetworkRequestEnterpriseConnector" policy.
//
// Only audit-only policies are supported, meaning the network request is
// never delayed or blocked by the scan: the verdict is only used for
// reporting. As such, `ReportWarningBypass()` should never be called since no
// warning can be shown for a network request.
class NetworkRequestHandler : public RequestHandlerBase {
 public:
  using CompletionCallback = base::OnceCallback<void(RequestHandlerResult)>;

  // `content_analysis_info` must outlive this object and have cloud analysis
  // settings that don't block until a verdict is obtained. `upload_service`
  // can be null, in which case the scan completes with an error. `url` is the
  // URL the network request is sent to, and `request_body` is the body of that
  // request. `router` can be null, in which case no report is sent for the
  // verdict. `callback` is called once the verdict is obtained and reported,
  // or once the scan fails. `policy_connector_getter` is forwarded to the
  // `NetworkRequestAnalysisRequest` created for the scan.
  NetworkRequestHandler(
      ContentAnalysisInfoBase* content_analysis_info,
      BinaryUploadService* upload_service,
      ReportingEventRouter* router,
      GURL url,
      scoped_refptr<network::ResourceRequestBody> request_body,
      CompletionCallback callback,
      BinaryUploadRequest::BrowserPolicyConnectorGetter
          policy_connector_getter);
  ~NetworkRequestHandler() override;

  NetworkRequestHandler(const NetworkRequestHandler&) = delete;
  NetworkRequestHandler& operator=(const NetworkRequestHandler&) = delete;

  // Called after obtaining a response from `BinaryUploadService`.
  void OnContentAnalysisResponse(ScanRequestUploadResult result,
                                 ContentAnalysisResponse response);

  // RequestHandlerBase:
  void ReportWarningBypass(
      std::optional<std::u16string> user_justification) override;

 protected:
  // Calls `BinaryUploadService` with the passed request to obtain a scanning
  // response, or completes the scan with an error asynchronously if there is
  // no `BinaryUploadService`. Virtual for tests.
  virtual void UploadForDeepScanning(
      std::unique_ptr<NetworkRequestAnalysisRequest> request);

 private:
  // RequestHandlerBase:
  bool UploadDataImpl() override;

  // Called once the size of the request body is known, and uploads `request`
  // for deep scanning.
  void OnGotRequestData(std::unique_ptr<NetworkRequestAnalysisRequest> request,
                        ScanRequestUploadResult result,
                        BinaryUploadRequest::Data data);

  // The body of the network request. This is moved to the
  // `NetworkRequestAnalysisRequest` when uploading starts.
  scoped_refptr<network::ResourceRequestBody> request_body_;

  // The size of the body of the network request, or -1 if it's unknown.
  int64_t content_size_ = -1;

  raw_ptr<ReportingEventRouter> reporting_event_router_ = nullptr;

  // Called after a response has been obtained from scanning, or if an error
  // `ScanRequestUploadResult` was received.
  CompletionCallback callback_;

  BinaryUploadRequest::BrowserPolicyConnectorGetter policy_connector_getter_;

  base::WeakPtrFactory<NetworkRequestHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_HANDLER_H_
