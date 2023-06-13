// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_report_controller.h"

#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_uploader.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/dm_token.h"

namespace enterprise_reporting {

namespace {
void OnExtensionRequestEnqueued(bool success) {
  // So far, there is nothing handle the enqueue failure as the CBCM status
  // report will cover all failed requests. However, we may need a retry logic
  // here if Extension workflow is decoupled from the status report.
  if (!success) {
    LOG(ERROR) << "Extension request failed to be added to the pipeline.";
  }
}

}  // namespace

RealTimeReportController::RealTimeReportController(
    ReportingDelegateFactory* delegate_factory)
    : real_time_report_generator_(
          std::make_unique<RealTimeReportGenerator>(delegate_factory)),
      delegate_(delegate_factory->GetRealTimeReportControllerDelegate()) {
  if (delegate_) {
    delegate_->SetTriggerCallback(
        base::BindRepeating(&RealTimeReportController::GenerateAndUploadReport,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}
RealTimeReportController::~RealTimeReportController() = default;

RealTimeReportController::Delegate::Delegate() = default;
RealTimeReportController::Delegate::~Delegate() = default;

void RealTimeReportController::Delegate::SetTriggerCallback(
    RealTimeReportController::TriggerCallback callback) {
  DCHECK(!trigger_callback_);
  DCHECK(callback);
  trigger_callback_ = std::move(callback);
}

void RealTimeReportController::OnDMTokenUpdated(policy::DMToken&& dm_token) {
  if (!delegate_) {
    return;
  }

  dm_token_ = dm_token;
  if (dm_token_.is_valid()) {
    delegate_->StartWatchingExtensionRequestIfNeeded();
  } else {
    delegate_->StopWatchingExtensionRequest();
    extension_request_uploader_.reset();
  }
}

void RealTimeReportController::GenerateAndUploadReport(
    ReportTrigger trigger,
    const RealTimeReportGenerator::Data& data) {
  if (!dm_token_.is_valid()) {
    return;
  }

  if (trigger == RealTimeReportController::ReportTrigger::kExtensionRequest) {
    UploadExtensionRequests(data);
  }
}

void RealTimeReportController::UploadExtensionRequests(
    const RealTimeReportGenerator::Data& data) {
  DCHECK(real_time_report_generator_);
  VLOG(1) << "Create extension request and add it to the pipeline.";

  if (!dm_token_.is_valid()) {
    return;
  }

  if (!extension_request_uploader_) {
    extension_request_uploader_ = RealTimeUploader::Create(
        dm_token_.value(), reporting::Destination::EXTENSIONS_WORKFLOW,
        reporting::Priority::FAST_BATCH);
  }
  auto reports = real_time_report_generator_->Generate(
      RealTimeReportGenerator::ReportType::kExtensionRequest, data);

  for (auto& report : reports) {
    extension_request_uploader_->Upload(
        std::move(report), base::BindOnce(&OnExtensionRequestEnqueued));
  }
}

void RealTimeReportController::SetExtensionRequestUploaderForTesting(
    std::unique_ptr<RealTimeUploader> uploader) {
  extension_request_uploader_ = std::move(uploader);
}

void RealTimeReportController::SetReportGeneratorForTesting(
    std::unique_ptr<RealTimeReportGenerator> generator) {
  real_time_report_generator_ = std::move(generator);
}

RealTimeReportController::Delegate*
RealTimeReportController::GetDelegateForTesting() {
  return delegate_.get();
}

}  // namespace enterprise_reporting
