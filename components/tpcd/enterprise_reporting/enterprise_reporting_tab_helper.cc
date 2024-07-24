// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/enterprise_reporting/enterprise_reporting_tab_helper.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace tpcd::enterprise_reporting {

constexpr char kEnterpriseErrorReportType[] =
    "enterprise-third-party-cookie-access-error";
constexpr char kEnterpriseWarningReportType[] =
    "enterprise-third-party-cookie-access-warning";

struct ReportDetails {
  std::string frame_url;
  std::string access_url;
  std::string name;
  std::string domain;
  std::string path;
  std::string access_operation;
};

base::Value::Dict CreateReportBody(const ReportDetails& report_details) {
  base::Value::Dict body;

  body.Set("frameUrl", report_details.frame_url);
  body.Set("accessUrl", report_details.access_url);
  body.Set("name", report_details.name);
  body.Set("domain", report_details.domain);
  body.Set("path", report_details.path);
  body.Set("accessOperation", report_details.access_operation);

  return body;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(EnterpriseReportingTabHelper);

EnterpriseReportingTabHelper::EnterpriseReportingTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<EnterpriseReportingTabHelper>(
          *web_contents) {}

EnterpriseReportingTabHelper::~EnterpriseReportingTabHelper() = default;

void EnterpriseReportingTabHelper::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (!base::FeatureList::IsEnabled(
          net::features::kReportingApiEnableEnterpriseCookieIssues)) {
    return;
  }
  network::mojom::NetworkContext* network_context =
      render_frame_host->GetStoragePartition()->GetNetworkContext();

  std::string access_operation =
      details.type == network::mojom::CookieAccessDetails_Type::kRead ? "read"
                                                                      : "write";

  for (const net::CookieWithAccessResult& cookie :
       details.cookie_access_result_list) {
    const net::CookieInclusionStatus& status = cookie.access_result.status;
    std::string report_type;
    if (status.HasExclusionReason(
            net::CookieInclusionStatus::
                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET) ||
        status.HasExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
      report_type = kEnterpriseErrorReportType;
    } else if (status.HasWarningReason(
                   net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)) {
      report_type = kEnterpriseWarningReportType;
    } else {
      continue;
    }

    ReportDetails report_details(
        render_frame_host->GetLastCommittedURL().spec(), details.url.spec(),
        cookie.cookie.Name(), cookie.cookie.Domain(), cookie.cookie.Path(),
        access_operation);

    // TODO(crbug.com/352737473): Update group parameter to use endpoint from
    // subscription.
    network_context->QueueEnterpriseReport(
        report_type, report_type,
        render_frame_host->GetOutermostMainFrameOrEmbedder()
            ->GetLastCommittedURL(),
        CreateReportBody(report_details));
  }
}

}  // namespace tpcd::enterprise_reporting
