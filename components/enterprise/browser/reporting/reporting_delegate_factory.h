// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_DELEGATE_FACTORY_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_DELEGATE_FACTORY_H_

#include <memory>

#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"

namespace enterprise_reporting {

// Abstract factory to create platform-specific reporting classes at runtime.
class ReportingDelegateFactory {
 public:
  ReportingDelegateFactory() = default;
  ReportingDelegateFactory(const ReportingDelegateFactory&) = delete;
  ReportingDelegateFactory& operator=(const ReportingDelegateFactory&) = delete;
  virtual ~ReportingDelegateFactory() = default;

  virtual std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() const = 0;

  virtual std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() const = 0;

  virtual std::unique_ptr<ReportGenerator::Delegate>
  GetReportGeneratorDelegate() const = 0;

  virtual std::unique_ptr<ReportScheduler::Delegate>
  GetReportSchedulerDelegate() const = 0;

  virtual std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() const = 0;

  virtual std::unique_ptr<RealTimeReportController::Delegate>
  GetRealTimeReportControllerDelegate() const = 0;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_DELEGATE_FACTORY_H_
