// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"

#include <memory>
#include <utility>

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#include "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

namespace {

using ReportEvent = ::chrome::cros::reporting::proto::SaasUsageReportEvent;

}  // namespace

SaasUsageReportFactory::SaasUsageReportFactory(
    PrefService* pref_service,
    std::unique_ptr<Delegate> delegate)
    : pref_service_(*pref_service), delegate_(std::move(delegate)) {}

SaasUsageReportFactory::~SaasUsageReportFactory() = default;

ReportEvent SaasUsageReportFactory::CreateReport() {
  ReportEvent report;
  auto profile_id = delegate_->GetProfileId();
  if (profile_id.has_value()) {
    report.set_profile_id(*profile_id);
    report.set_profile_affiliated(delegate_->IsProfileAffiliated());
  }

  PopulateSaasUsageDomainMetrics(pref_service_.get(), report);
  return report;
}

}  // namespace enterprise_reporting
