// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/ping_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("safe_browsing_extended_reporting",
                                        R"(
      semantics {
        sender: "Safe Browsing Extended Reporting"
        description:
          "When a user is opted in to automatically reporting 'possible "
          "security incidents to Google,' and they reach a bad page that's "
          "flagged by Safe Browsing, Chrome will send a report to Google "
          "with information about the threat. This helps Safe Browsing learn "
          "where threats originate and thus protect more users."
        trigger:
          "When a red interstitial is shown, and the user is opted-in."
        data:
          "The report includes the URL and referrer chain of the page. If the "
          "warning is triggered by a subresource on a partially loaded page, "
          "the report will include the URL and referrer chain of sub frames "
          "and resources loaded into the page.  It may also include a subset "
          "of headers for resources loaded, and some Google ad identifiers to "
          "help block malicious ads."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "Safe Browsing Cookie Store"
        setting:
          "Users can control this feature via the 'Automatically report "
          "details of possible security incidents to Google' setting under "
          "'Privacy'. The feature is disabled by default."
        chrome_policy {
          SafeBrowsingExtendedReportingOptInAllowed {
            policy_options {mode: MANDATORY}
            SafeBrowsingExtendedReportingOptInAllowed: false
          }
        }
      })");

}  // namespace

namespace safe_browsing {

// SafeBrowsingPingManager implementation ----------------------------------

// static
std::unique_ptr<PingManager> PingManager::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  return base::WrapUnique(new PingManager(url_loader_factory, config));
}

PingManager::PingManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config)
    : config_(config), url_loader_factory_(url_loader_factory) {}

PingManager::~PingManager() {}

// All SafeBrowsing request responses are handled here.
void PingManager::OnURLLoaderComplete(
    network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  auto it = safebrowsing_reports_.find(source);
  DCHECK(it != safebrowsing_reports_.end());
  safebrowsing_reports_.erase(it);
}

// Sends a SafeBrowsing "hit" report.
void PingManager::ReportSafeBrowsingHit(
    const safe_browsing::HitReport& hit_report) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  GURL report_url = SafeBrowsingHitUrl(hit_report);
  resource_request->url = report_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  if (!hit_report.post_data.empty())
    resource_request->method = "POST";

  auto report_ptr = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);

  if (!hit_report.post_data.empty())
    report_ptr->AttachStringForUpload(hit_report.post_data, "text/plain");

  report_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PingManager::OnURLLoaderComplete, base::Unretained(this),
                     report_ptr.get()));
  safebrowsing_reports_.insert(std::move(report_ptr));
}

// Sends threat details for users who opt-in.
void PingManager::ReportThreatDetails(const std::string& report) {
  GURL report_url = ThreatDetailsUrl();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = report_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);

  loader->AttachStringForUpload(report, "application/octet-stream");

  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PingManager::OnURLLoaderComplete, base::Unretained(this),
                     loader.get()));
  safebrowsing_reports_.insert(std::move(loader));
}

GURL PingManager::SafeBrowsingHitUrl(
    const safe_browsing::HitReport& hit_report) const {
  DCHECK(hit_report.threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_BINARY_MALWARE ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING ||
         hit_report.threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE);
  std::string url =
      GetReportUrl(config_, "report", &hit_report.extended_reporting_level);
  std::string threat_list = "none";
  switch (hit_report.threat_type) {
    case SB_THREAT_TYPE_URL_MALWARE:
      threat_list = "malblhit";
      break;
    case SB_THREAT_TYPE_URL_PHISHING:
      threat_list = "phishblhit";
      break;
    case SB_THREAT_TYPE_URL_UNWANTED:
      threat_list = "uwsblhit";
      break;
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
      threat_list = "binurlhit";
      break;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      threat_list = "phishcsdhit";
      break;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      threat_list = "malcsdhit";
      break;
    default:
      NOTREACHED();
  }

  std::string threat_source = "none";
  switch (hit_report.threat_source) {
    case safe_browsing::ThreatSource::DATA_SAVER:
      threat_source = "ds";
      break;
    case safe_browsing::ThreatSource::REMOTE:
      threat_source = "rem";
      break;
    case safe_browsing::ThreatSource::LOCAL_PVER3:
      threat_source = "l3";
      break;
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      threat_source = "l4";
      break;
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      threat_source = "csd";
      break;
    case safe_browsing::ThreatSource::PASSWORD_PROTECTION_SERVICE:
      threat_source = "pps";
      break;
    case safe_browsing::ThreatSource::UNKNOWN:
      NOTREACHED();
  }

  // Add user_population component only if it's not empty.
  std::string user_population_comp;
  if (!hit_report.population_id.empty()) {
    // Population_id should be URL-safe, but escape it and size-limit it
    // anyway since it came from outside Chrome.
    std::string up_str =
        net::EscapeQueryParamValue(hit_report.population_id, true);
    if (up_str.size() > 512) {
      DCHECK(false) << "population_id is too long: " << up_str;
      up_str = "UP_STRING_TOO_LONG";
    }

    user_population_comp = "&up=" + up_str;
  }

  return GURL(base::StringPrintf(
      "%s&evts=%s&evtd=%s&evtr=%s&evhr=%s&evtb=%d&src=%s&m=%d%s", url.c_str(),
      threat_list.c_str(),
      net::EscapeQueryParamValue(hit_report.malicious_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(hit_report.page_url.spec(), true).c_str(),
      net::EscapeQueryParamValue(hit_report.referrer_url.spec(), true).c_str(),
      hit_report.is_subresource, threat_source.c_str(),
      hit_report.is_metrics_reporting_active, user_population_comp.c_str()));
}

GURL PingManager::ThreatDetailsUrl() const {
  std::string url = GetReportUrl(config_, "clientreport/malware");
  return GURL(url);
}

}  // namespace safe_browsing
