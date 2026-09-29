// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_handler.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_analysis_request.h"
#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace enterprise_connectors {

NetworkRequestHandler::NetworkRequestHandler(
    ContentAnalysisInfoBase* content_analysis_info,
    BinaryUploadService* upload_service,
    ReportingEventRouter* router,
    GURL url,
    scoped_refptr<network::ResourceRequestBody> request_body,
    CompletionCallback callback,
    BinaryUploadRequest::BrowserPolicyConnectorGetter policy_connector_getter)
    : RequestHandlerBase(content_analysis_info,
                         upload_service,
                         std::move(url),
                         DeepScanAccessPoint::NETWORK_REQUEST),
      request_body_(std::move(request_body)),
      reporting_event_router_(router),
      callback_(std::move(callback)),
      policy_connector_getter_(std::move(policy_connector_getter)) {
  CHECK(request_body_);
}

NetworkRequestHandler::~NetworkRequestHandler() = default;

void NetworkRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  // Network request scans are audit-only, so no warning is ever shown.
  NOTREACHED();
}

void NetworkRequestHandler::UploadForDeepScanning(
    std::unique_ptr<NetworkRequestAnalysisRequest> request) {
  auto* upload_service = GetBinaryUploadService();
  if (!upload_service) {
    // `callback_` still needs to run so that the owner of `this` isn't left
    // waiting forever. It's run asynchronously so that `this` isn't deleted by
    // its owner while `UploadData()` is still on the stack.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&NetworkRequestHandler::OnContentAnalysisResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       ScanRequestUploadResult::kUnknown,
                       ContentAnalysisResponse()));
    return;
  }

  upload_service->MaybeUploadForDeepScanning(std::move(request));
}

bool NetworkRequestHandler::UploadDataImpl() {
  const AnalysisSettings& settings = content_analysis_info()->settings();

  // The "OnNetworkRequestEnterpriseConnector" policy only supports cloud
  // analysis, and only in audit-only mode.
  CHECK(settings.cloud_or_local_settings.is_cloud_analysis());
  CHECK_EQ(settings.block_until_verdict, BlockUntilVerdict::kNoBlock);

  auto request = std::make_unique<NetworkRequestAnalysisRequest>(
      settings.cloud_or_local_settings.cloud_settings(),
      std::move(request_body_),
      base::BindOnce(&NetworkRequestHandler::OnContentAnalysisResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(policy_connector_getter_));

  content_analysis_info()->InitializeRequest(
      request.get(), /*include_enterprise_only_fields=*/true);
  request->set_destination(url().spec());

  // The request is only uploaded once the size of its body is known. The
  // result of `GetRequestData()` is cached by the request, so the upload
  // service calling it again doesn't make it compute that size twice.
  NetworkRequestAnalysisRequest* request_raw = request.get();
  request_raw->GetRequestData(
      base::BindOnce(&NetworkRequestHandler::OnGotRequestData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));
  return true;
}

void NetworkRequestHandler::OnGotRequestData(
    std::unique_ptr<NetworkRequestAnalysisRequest> request,
    ScanRequestUploadResult result,
    BinaryUploadRequest::Data data) {
  // A size of 0 is only meaningful for a successful result. Otherwise, it
  // means the size of the body couldn't be computed (for example with chunked
  // bodies), so it's left as unknown.
  if (data.size != 0 || result == ScanRequestUploadResult::kSuccess) {
    content_size_ = base::saturated_cast<int64_t>(data.size);
  }

  // Unsuccessful results and empty bodies are left to the upload service to
  // handle, since it can still upload metadata for some of them (for example
  // with bodies that are too large).
  UploadForDeepScanning(std::move(request));
}

void NetworkRequestHandler::OnContentAnalysisResponse(
    ScanRequestUploadResult result,
    ContentAnalysisResponse response) {
  RecordDeepScanMetrics(/*is_cloud=*/true, access_point(),
                        base::TimeTicks::Now() - upload_start_time(),
                        content_size_, result, response);

  auto request_handler_result = CalculateRequestHandlerResult(
      content_analysis_info()->settings(), result, response);
  DVLOG(1) << __func__ << ": result=" << request_handler_result.complies;

  // Scans are audit-only, so the network request is always allowed regardless
  // of the verdict.
  MaybeReportDeepScanningVerdict(reporting_event_router_,
                                 content_analysis_info(),
                                 /*source=*/"",
                                 /*destination=*/url().spec(),
                                 /*file_name=*/"",
                                 /*sha256_or_cb=*/"",
                                 /*mime_type=*/"", access_point_string(),
                                 /*content_transfer_method=*/"",
                                 /*source_email=*/"", content_size_, result,
                                 response, EventResult::ALLOWED);

  std::move(callback_).Run(std::move(request_handler_result));
}

}  // namespace enterprise_connectors
