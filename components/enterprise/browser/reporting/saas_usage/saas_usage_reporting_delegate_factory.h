// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_H_

#include <memory>

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// Abstract factory to create platform-specific SaaS usage reporting classes at
// runtime.
class SaasUsageReportingDelegateFactory {
 public:
  virtual ~SaasUsageReportingDelegateFactory() = default;

  virtual PrefService* GetPrefService() const = 0;

  virtual std::unique_ptr<SaasUsageReportFactory::Delegate>
  GetSaasUsageReportFactoryDelegate() const = 0;

  virtual std::unique_ptr<SaasUsageReportUploader> GetSaasUsageReportUploader()
      const = 0;

  virtual std::unique_ptr<SaasUsageReportScheduler::Delegate>
  GetSaasUsageReportSchedulerDelegate() const = 0;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_H_
