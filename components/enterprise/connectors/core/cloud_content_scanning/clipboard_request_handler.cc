// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/clipboard_request_handler.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"

namespace enterprise_connectors {

namespace {

ClipboardRequestHandler::TestFactory* TestFactoryStorage() {
  static base::NoDestructor<ClipboardRequestHandler::TestFactory> factory;
  return factory.get();
}

}  // namespace

// static
std::unique_ptr<ClipboardRequestHandler> ClipboardRequestHandler::Create(
    ContentAnalysisInfoBase* content_analysis_info,
    BinaryUploadService* upload_service,
    ReportingEventRouter* router,
    GURL url,
    Type type,
    DeepScanAccessPoint access_point,
    ContentMetaData::CopiedTextSource clipboard_source,
    std::string source_content_area_email,
    std::string content_transfer_method,
    std::string data,
    CompletionCallback callback,
    BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter) {
  if (!TestFactoryStorage()->is_null()) {
    return TestFactoryStorage()->Run(
        content_analysis_info, upload_service, router, std::move(url), type,
        access_point, std::move(clipboard_source),
        std::move(source_content_area_email),
        std::move(content_transfer_method), std::move(data),
        std::move(callback), std::move(policy_getter));
  }
  return base::WrapUnique(new ClipboardRequestHandler(
      content_analysis_info, upload_service, router, std::move(url), type,
      access_point, std::move(clipboard_source),
      std::move(source_content_area_email), std::move(content_transfer_method),
      std::move(data), std::move(callback), std::move(policy_getter)));
}

// static
void ClipboardRequestHandler::SetFactoryForTesting(TestFactory factory) {
  *TestFactoryStorage() = std::move(factory);
}

// static
void ClipboardRequestHandler::ResetFactoryForTesting() {
  TestFactoryStorage()->Reset();
}

ClipboardRequestHandler::~ClipboardRequestHandler() = default;

ClipboardRequestHandler::ClipboardRequestHandler(
    ContentAnalysisInfoBase* content_analysis_info,
    BinaryUploadService* upload_service,
    ReportingEventRouter* router,
    GURL url,
    Type type,
    DeepScanAccessPoint access_point,
    ContentMetaData::CopiedTextSource clipboard_source,
    std::string source_content_area_email,
    std::string content_transfer_method,
    std::string data,
    CompletionCallback callback,
    BinaryUploadRequest::BrowserPolicyConnectorGetter policy_getter)
    : RequestHandlerBase(content_analysis_info,
                         upload_service,
                         std::move(url),
                         access_point),
      type_(type),
      data_(std::move(data)),
      content_size_(data_.size()),
      clipboard_source_(std::move(clipboard_source)),
      source_content_area_email_(std::move(source_content_area_email)),
      content_transfer_method_(std::move(content_transfer_method)),
      reporting_event_router_(router),
      callback_(std::move(callback)),
      browser_policy_getter_(std::move(policy_getter)) {}

void ClipboardRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  ReportAnalysisConnectorWarningBypass(
      reporting_event_router_, content_analysis_info(),
      /*source*/
      ReportingEventRouter::GetClipboardSourceString(clipboard_source_),
      /*destination*/ url().spec(),
      type_ == Type::kText ? "Text data" : "Image data",
      /*download_digest_sha256*/ "", type_ == Type::kText ? "text/plain" : "",
      access_point_string(), content_transfer_method_, content_size_, response_,
      user_justification);
}

std::string ClipboardRequestHandler::access_point_string() const {
  if (access_point() == DeepScanAccessPoint::COPY) {
    return kClipboardCopyDataTransferEventTrigger;
  }
  return kWebContentUploadDataTransferEventTrigger;
}

void ClipboardRequestHandler::UploadForDeepScanning(
    std::unique_ptr<ClipboardAnalysisRequest> request) {
  auto* upload_service = GetBinaryUploadService();
  if (upload_service) {
    upload_service->MaybeUploadForDeepScanning(std::move(request));
  }
}

bool ClipboardRequestHandler::UploadDataImpl() {
  auto request = std::make_unique<ClipboardAnalysisRequest>(
      content_analysis_info()->settings().cloud_or_local_settings,
      std::move(data_),
      base::BindOnce(&ClipboardRequestHandler::OnContentAnalysisResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(browser_policy_getter_));

  content_analysis_info()->InitializeRequest(
      request.get(), /*include_enterprise_only_fields=*/true);
  if (access_point() == DeepScanAccessPoint::COPY) {
    request->set_analysis_connector(DATA_COPIED);
  } else {
    request->set_analysis_connector(BULK_DATA_ENTRY);
  }
  if (type_ == Type::kImage) {
    request->set_image_paste(true);
  }
  if (type_ == Type::kText) {
    request->set_destination(url().spec());
    std::string source_string =
        ReportingEventRouter::GetClipboardSourceString(clipboard_source_);
    if (!source_string.empty()) {
      request->set_source(source_string);
    }
    if (clipboard_source_.has_context()) {
      request->set_clipboard_source_type(clipboard_source_.context());
    }
    if (clipboard_source_.has_url()) {
      request->set_clipboard_source_url(clipboard_source_.url());
    }
  }
  if (!source_content_area_email_.empty()) {
    request->set_source_content_area_account_email(source_content_area_email_);
  }

  UploadForDeepScanning(std::move(request));
  return true;
}

void ClipboardRequestHandler::OnContentAnalysisResponse(
    ScanRequestUploadResult result,
    ContentAnalysisResponse response) {
  response_ = std::move(response);
  AddRequestTokenToAckFinalAction(response_.request_token(),
                                  GetAckFinalAction(response_));

  RecordDeepScanMetrics(content_analysis_info()
                            ->settings()
                            .cloud_or_local_settings.is_cloud_analysis(),
                        access_point(),
                        base::TimeTicks::Now() - upload_start_time(),
                        content_size_, result, response_);

  auto request_handler_result = CalculateRequestHandlerResult(
      content_analysis_info()->settings(), result, response_);
  DVLOG(1) << __func__
           << (type_ == Type::kText ? ": text result=" : ": image result=")
           << request_handler_result.complies;

  bool should_warn = request_handler_result.final_result ==
                     FinalContentAnalysisResult::WARNING;

  MaybeReportDeepScanningVerdict(
      reporting_event_router_, content_analysis_info(),
      /*source*/
      ReportingEventRouter::GetClipboardSourceString(clipboard_source_),
      /*destination*/ url().spec(),
      type_ == Type::kText ? "Text data" : "Image data",
      /*download_digest_sha256*/ "", type_ == Type::kText ? "text/plain" : "",
      access_point_string(), content_transfer_method_,
      source_content_area_email_, content_size_, result, response_,
      CalculateEventResult(content_analysis_info()->settings(),
                           request_handler_result.complies, should_warn,
                           result));

  std::move(callback_).Run(std::move(request_handler_result));
}

}  // namespace enterprise_connectors
