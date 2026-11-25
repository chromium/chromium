// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_HISTOGRAM_ENTERPRISE_REPORTED_HISTOGRAMS_ALLOWLIST_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_HISTOGRAM_ENTERPRISE_REPORTED_HISTOGRAMS_ALLOWLIST_H_

#include <string_view>

namespace enterprise_reporting {

// Allowlist of UMA histograms uploaded via Chrome Enterprise Reporting
inline constexpr std::string_view kEnterpriseReportedHistograms[] = {
    // keep-sorted start
    "DataTransferReportingTimeDiff",
    "Enterprise.ExtensibleEnterpriseSSO.Okta.Failure.Duration",
    "Enterprise.ExtensibleEnterpriseSSO.Okta.Failure.Reason",
    "Enterprise.ExtensibleEnterpriseSSO.Okta.Result",
    "Enterprise.ExtensibleEnterpriseSSO.Okta.Success.Duration",
    "Enterprise.ReportingEvent.All.UploadFailure.Duration",
    "Enterprise.ReportingEvent.All.UploadSuccess.Duration",
    "SafeBrowsing.RT.GetCache.Time.Enterprise",
    "SafeBrowsing.RT.GetToken.TimeTaken.Enterprise",
    // keep-sorted end
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_HISTOGRAM_ENTERPRISE_REPORTED_HISTOGRAMS_ALLOWLIST_H_
