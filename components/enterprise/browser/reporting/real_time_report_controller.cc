// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/real_time_report_controller.h"

#include "base/containers/flat_map.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"
#include "components/enterprise/browser/reporting/real_time_uploader.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/dm_token.h"

namespace enterprise_reporting {

namespace {

void OnReportEnqueued(RealTimeReportType type, bool success) {
  // So far, there is nothing handle the enqueue failure as the CBCM status
  // report will cover all failed requests. However, we may need a retry logic
  // in the future.
  LOG_IF(ERROR, !success)
      << "Real time request failed to be added to the pipeline: " << type;
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
    report_uploaders_.clear();
  }
}

void RealTimeReportController::GenerateAndUploadReport(
    RealTimeReportType type,
    const RealTimeReportGenerator::Data& data) {
  if (!dm_token_.is_valid()) {
    return;
  }

  static const base::flat_map<RealTimeReportType, ReportConfig> kConfigs = {
      {RealTimeReportType::kExtensionRequest,
       {RealTimeReportType::kExtensionRequest,
        reporting::Destination::EXTENSIONS_WORKFLOW,
        reporting::Priority::FAST_BATCH}},
      {RealTimeReportType::kLegacyTech,
       {RealTimeReportType::kLegacyTech, reporting::Destination::LEGACY_TECH,
        reporting::Priority::BACKGROUND_BATCH}},
  };

  UploadReport(data, kConfigs.at(type));
}

void RealTimeReportController::UploadReport(
    const RealTimeReportGenerator::Data& data,
    const ReportConfig& config) {
  DCHECK(real_time_report_generator_);

  VLOG(1) << "Create real time event and add it to the pipeline: "
          << config.type;

  if (!dm_token_.is_valid()) {
    return;
  }

  if (!report_uploaders_.contains(config.type)) {
    report_uploaders_[config.type] = RealTimeUploader::Create(
        dm_token_.value(), config.destination, config.priority);
  }
  auto* uploader = report_uploaders_[config.type].get();

  auto reports = real_time_report_generator_->Generate(config.type, data);

  for (auto& report : reports) {
    uploader->Upload(std::move(report),
                     base::BindOnce(&OnReportEnqueued, config.type));
  }
}

void RealTimeReportController::SetUploaderForTesting(
    RealTimeReportType type,
    std::unique_ptr<RealTimeUploader> uploader) {
  report_uploaders_[type] = std::move(uploader);
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
