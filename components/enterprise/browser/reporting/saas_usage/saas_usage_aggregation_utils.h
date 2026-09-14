// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_AGGREGATION_UTILS_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_AGGREGATION_UTILS_H_

#include <optional>
#include <string_view>

#include "components/enterprise/common/proto/synced/saas_usage_report_event.pb.h"
#include "components/prefs/pref_service.h"
#include "net/ssl/ssl_connection_status_flags.h"

namespace enterprise_reporting {

// Returns the string representation of an SSL/TLS version for SaaS usage
// reporting (e.g. "TLS 1.3", "TLS 1.2", "Unknown", "Unencrypted").
std::string_view GetEncryptionProtocolString(
    std::optional<net::SSLVersion> ssl_version);

// These functions are used to aggregate domain reporting data. They store the
// aggregated data using PrefService.

void RecordNavigation(PrefService& pref_service,
                      std::string_view domain,
                      std::string_view encryption_protocol);

void PopulateSaasUsageDomainMetrics(
    PrefService& pref_service,
    ::chrome::cros::reporting::proto::SaasUsageReportEvent& report);

void ClearSaasUsageReport(PrefService& pref_service);

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_AGGREGATION_UTILS_H_
