// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

class SaasUsageReportingDelegateFactory;

// Schedules generation and upload of SaaS usage reports.
//
// Reports are generated and uploaded every 4 hours, provided the delegate
// indicates readiness and the generated report is not empty. The scheduler
// is notified by the delegate when state changes to start or stop the periodic
// timer.
class SaasUsageReportScheduler {
 public:
  static constexpr base::TimeDelta kReportInterval = base::Hours(4);

  // The delegate abstracts the conditions required for reporting, such as the
  // presence of at least one profile for browser-level reporting on desktop.
  // Profile-level reporting does not use this delegate as the profile is
  // always ready to generate reports.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SetReadyStateChangedCallback(
        base::RepeatingClosure callback) = 0;
    virtual bool IsReady() = 0;
  };

  static std::unique_ptr<SaasUsageReportScheduler> Create(
      const SaasUsageReportingDelegateFactory* delegate_factory);

  explicit SaasUsageReportScheduler(
      PrefService* pref_service,
      std::unique_ptr<SaasUsageReportFactory> report_factory,
      std::unique_ptr<SaasUsageReportUploader> report_uploader,
      std::unique_ptr<Delegate> delegate);
  SaasUsageReportScheduler(const SaasUsageReportScheduler&) = delete;
  SaasUsageReportScheduler& operator=(const SaasUsageReportScheduler&) = delete;
  ~SaasUsageReportScheduler();

  void TriggerReport();

 private:
  void GenerateAndUploadReport();
  void ScheduleNextReport();
  void OnReadyStateChanged();
  void OnReportUploaded(bool success);

  base::WallClockTimer timer_;
  raw_ref<PrefService> pref_service_;
  std::unique_ptr<SaasUsageReportFactory> report_factory_;
  std::unique_ptr<SaasUsageReportUploader> report_uploader_;
  std::unique_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SaasUsageReportScheduler> weak_ptr_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_H_
