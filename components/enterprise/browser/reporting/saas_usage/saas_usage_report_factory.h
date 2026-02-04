// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// This class is responsible for creating a SaasUsageReportEvent from using
// aggregated data from the PrefService and profile information from the
// delegate.
class SaasUsageReportFactory {
 public:
  // Delegate to get profile-specific information from platform-specific
  // implementations.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual std::optional<std::string> GetProfileId() = 0;
    virtual bool IsProfileAffiliated() = 0;
  };

  explicit SaasUsageReportFactory(PrefService* pref_service,
                                  std::unique_ptr<Delegate> delegate);
  SaasUsageReportFactory(const SaasUsageReportFactory&) = delete;
  SaasUsageReportFactory& operator=(const SaasUsageReportFactory&) = delete;
  ~SaasUsageReportFactory();

  ::chrome::cros::reporting::proto::SaasUsageReportEvent CreateReport();

 private:
  raw_ref<PrefService> pref_service_;
  std::unique_ptr<Delegate> delegate_;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_H_
