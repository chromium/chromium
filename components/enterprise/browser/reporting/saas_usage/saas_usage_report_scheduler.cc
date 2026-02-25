// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_delegate_factory.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// static
std::unique_ptr<SaasUsageReportScheduler> SaasUsageReportScheduler::Create(
    const SaasUsageReportingDelegateFactory* delegate_factory) {
  PrefService* pref_service = delegate_factory->GetPrefService();
  return std::make_unique<SaasUsageReportScheduler>(
      pref_service,
      std::make_unique<SaasUsageReportFactory>(
          pref_service, delegate_factory->GetSaasUsageReportFactoryDelegate()),
      delegate_factory->GetSaasUsageReportUploader(),
      delegate_factory->GetSaasUsageReportSchedulerDelegate());
}

SaasUsageReportScheduler::SaasUsageReportScheduler(
    PrefService* pref_service,
    std::unique_ptr<SaasUsageReportFactory> report_factory,
    std::unique_ptr<SaasUsageReportUploader> report_uploader,
    std::unique_ptr<Delegate> delegate)
    : pref_service_(raw_ref<PrefService>::from_ptr(pref_service)),
      report_factory_(std::move(report_factory)),
      report_uploader_(std::move(report_uploader)),
      delegate_(std::move(delegate)) {
  CHECK(report_factory_);
  CHECK(report_uploader_);

  if (delegate_) {
    delegate_->SetReadyStateChangedCallback(
        base::BindRepeating(&SaasUsageReportScheduler::OnReadyStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    // Notify the scheduler of the initial ready state.
    OnReadyStateChanged();
  } else {
    ScheduleNextReport();
  }
}

SaasUsageReportScheduler::~SaasUsageReportScheduler() = default;

void SaasUsageReportScheduler::TriggerReport() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GenerateAndUploadReport();
  pref_service_->SetTime(kSaasUsageReportLastTriggerTime, base::Time::Now());
  ScheduleNextReport();
}

void SaasUsageReportScheduler::GenerateAndUploadReport() {
  auto report = report_factory_->CreateReport();
  if (report.domain_metrics().empty()) {
    VLOG_POLICY(1, REPORTING)
        << "No domain metrics aggregated - skipping report upload.";
    return;
  }
  report_uploader_->UploadReport(
      report, base::BindOnce(&SaasUsageReportScheduler::OnReportUploaded,
                             weak_ptr_factory_.GetWeakPtr()));
}

void SaasUsageReportScheduler::OnReportUploaded(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    ClearSaasUsageReport(pref_service_.get());
  } else {
    LOG_POLICY(WARNING, REPORTING)
        << "SaaS usage report upload failed, upload will be retried at the "
           "next trigger.";
  }
}

void SaasUsageReportScheduler::ScheduleNextReport() {
  const base::Time last_trigger_time =
      pref_service_->GetTime(kSaasUsageReportLastTriggerTime);
  const base::Time now = base::Time::Now();
  // Last trigger time can be in future (e.g. timezone change), so we take the
  // minimum of last trigger time and now for calculating the next trigger
  // time.
  const base::Time next_trigger_time =
      std::min(last_trigger_time, now) + kReportInterval;

  // If the next trigger time is in the past, WallClockTimer will trigger the
  // report immediately.
  VLOG_POLICY(1, REPORTING)
      << "Scheduling next SaaS usage report at " << next_trigger_time;
  timer_.Start(FROM_HERE, next_trigger_time, this,
               &SaasUsageReportScheduler::TriggerReport);
}

void SaasUsageReportScheduler::OnReadyStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(delegate_) << "Delegate is null, but OnReadyStateChanged is called.";

  // Schedule next report if delegate is ready, otherwise stop the timer.
  if (delegate_->IsReady()) {
    ScheduleNextReport();
  } else {
    timer_.Stop();
  }
}

}  // namespace enterprise_reporting
