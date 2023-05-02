// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/ping_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

GURL GetSanitizedUrl(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  return url.ReplaceComponents(replacements);
}
std::string GetSanitizedUrl(const std::string& url_spec) {
  GURL url = GURL(url_spec);
  return GetSanitizedUrl(url).spec();
}

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
          SafeBrowsingExtendedReportingEnabled {
            policy_options {mode: MANDATORY}
            SafeBrowsingExtendedReportingEnabled: false
          }
        }
      })");

}  // namespace

namespace safe_browsing {

// SafeBrowsingPingManager implementation ----------------------------------

// static
PingManager* PingManager::Create(
    const V4ProtocolConfig& config,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    base::RepeatingCallback<bool()> get_should_fetch_access_token,
    WebUIDelegate* webui_delegate,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::RepeatingCallback<ChromeUserPopulation()>
        get_user_population_callback,
    base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>
        get_page_load_token_callback) {
  return new PingManager(config, url_loader_factory, std::move(token_fetcher),
                         get_should_fetch_access_token, webui_delegate,
                         ui_task_runner, get_user_population_callback,
                         get_page_load_token_callback);
}

PingManager::PingManager(
    const V4ProtocolConfig& config,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    base::RepeatingCallback<bool()> get_should_fetch_access_token,
    WebUIDelegate* webui_delegate,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::RepeatingCallback<ChromeUserPopulation()>
        get_user_population_callback,
    base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>
        get_page_load_token_callback)
    : config_(config),
      url_loader_factory_(url_loader_factory),
      token_fetcher_(std::move(token_fetcher)),
      get_should_fetch_access_token_(get_should_fetch_access_token),
      webui_delegate_(webui_delegate),
      ui_task_runner_(ui_task_runner),
      get_user_population_callback_(get_user_population_callback),
      get_page_load_token_callback_(get_page_load_token_callback) {}

PingManager::~PingManager() {}

// All SafeBrowsing request responses are handled here.
void PingManager::OnURLLoaderComplete(
    network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  auto it = safebrowsing_reports_.find(source);
  DCHECK(it != safebrowsing_reports_.end());
  safebrowsing_reports_.erase(it);
}

void PingManager::OnThreatDetailsReportURLLoaderComplete(
    network::SimpleURLLoader* source,
    bool has_access_token,
    std::unique_ptr<std::string> response_body) {
  int response_code = source->ResponseInfo() && source->ResponseInfo()->headers
                          ? source->ResponseInfo()->headers->response_code()
                          : 0;
  std::string metric = "SafeBrowsing.ClientSafeBrowsingReport.NetworkResult.";
  std::string suffix = (has_access_token ? "YesAccessToken" : "NoAccessToken");
  RecordHttpResponseOrErrorCode((metric + suffix).c_str(), source->NetError(),
                                response_code);

  OnURLLoaderComplete(source, std::move(response_body));
}

// Sends a SafeBrowsing "hit" report.
void PingManager::ReportSafeBrowsingHit(
    std::unique_ptr<safe_browsing::HitReport> hit_report) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  SanitizeHitReport(hit_report.get());
  GURL report_url = SafeBrowsingHitUrl(hit_report.get());
  resource_request->url = report_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  if (!hit_report->post_data.empty()) {
    resource_request->method = "POST";
  }

  auto report_ptr = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);

  if (!hit_report->post_data.empty()) {
    report_ptr->AttachStringForUpload(hit_report->post_data, "text/plain");
  }

  report_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PingManager::OnURLLoaderComplete, base::Unretained(this),
                     report_ptr.get()));
  safebrowsing_reports_.insert(std::move(report_ptr));

  // The following is to log this HitReport on any open chrome://safe-browsing
  // pages.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIDelegate::AddToHitReportsSent,
                     // Unretained is okay because in practice, webui_delegate_
                     // is a singleton.
                     base::Unretained(webui_delegate_), std::move(hit_report)));
}

// Sends threat details for users who opt-in.
PingManager::ReportThreatDetailsResult PingManager::ReportThreatDetails(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report,
    bool attach_default_data) {
  SanitizeThreatDetailsReport(report.get());
  if (attach_default_data) {
    if (!get_user_population_callback_.is_null()) {
      *report->mutable_population() = get_user_population_callback_.Run();
    }
    if (!get_page_load_token_callback_.is_null()) {
      ChromeUserPopulation::PageLoadToken token =
          get_page_load_token_callback_.Run(GURL(report->page_url()));
      base::UmaHistogramBoolean(
          "SafeBrowsing.ClientSafeBrowsingReport.IsPageLoadTokenNull",
          !token.has_token_value());
      report->mutable_population()->mutable_page_load_tokens()->Add()->Swap(
          &token);
    }
  }

  std::string serialized_report;
  if (!report->SerializeToString(&serialized_report)) {
    DLOG(ERROR) << "Unable to serialize the threat report.";
    return ReportThreatDetailsResult::SERIALIZATION_ERROR;
  }
  if (serialized_report.empty()) {
    DLOG(ERROR) << "The threat report is empty.";
    return ReportThreatDetailsResult::EMPTY_REPORT;
  }

  if (attach_default_data && get_should_fetch_access_token_.Run()) {
    token_fetcher_->Start(
        base::BindOnce(&PingManager::ReportThreatDetailsOnGotAccessToken,
                       weak_factory_.GetWeakPtr(), serialized_report));
  } else {
    std::string empty_access_token;
    ReportThreatDetailsOnGotAccessToken(serialized_report, empty_access_token);
  }

  base::UmaHistogramExactLinear(
      "SafeBrowsing.ClientSafeBrowsingReport.ReportType", report->type(),
      ClientSafeBrowsingReportRequest::ReportType_MAX + 1);
  // The following is to log this ClientSafeBrowsingReportRequest on any open
  // chrome://safe-browsing pages.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIDelegate::AddToCSBRRsSent,
                     // Unretained is okay because in practice, webui_delegate_
                     // is a singleton
                     base::Unretained(webui_delegate_), std::move(report)));

  return ReportThreatDetailsResult::SUCCESS;
}

void PingManager::ReportThreatDetailsOnGotAccessToken(
    const std::string& serialized_report,
    const std::string& access_token) {
  GURL report_url = ThreatDetailsUrl();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = report_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->method = "POST";

  if (!access_token.empty()) {
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token);
  }
  base::UmaHistogramBoolean(
      "SafeBrowsing.ClientSafeBrowsingReport.RequestHasToken",
      !access_token.empty());

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);

  loader->AttachStringForUpload(serialized_report, "application/octet-stream");

  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PingManager::OnThreatDetailsReportURLLoaderComplete,
                     base::Unretained(this), loader.get(),
                     !access_token.empty()));
  safebrowsing_reports_.insert(std::move(loader));
}

GURL PingManager::SafeBrowsingHitUrl(
    safe_browsing::HitReport* hit_report) const {
  DCHECK(hit_report->threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_BINARY_MALWARE ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE);
  std::string url =
      GetReportUrl(config_, "report", &hit_report->extended_reporting_level,
                   hit_report->is_enhanced_protection);
  std::string threat_list = "none";
  switch (hit_report->threat_type) {
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
  switch (hit_report->threat_source) {
    case safe_browsing::ThreatSource::REMOTE:
      threat_source = "rem";
      break;
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      threat_source = "l4";
      break;
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      threat_source = "csd";
      break;
    case safe_browsing::ThreatSource::URL_REAL_TIME_CHECK:
      threat_source = "rt";
      break;
    case safe_browsing::ThreatSource::UNKNOWN:
      NOTREACHED();
  }

  // Add user_population component only if it's not empty.
  std::string user_population_comp;
  if (!hit_report->population_id.empty()) {
    // Population_id should be URL-safe, but escape it and size-limit it
    // anyway since it came from outside Chrome.
    std::string up_str =
        base::EscapeQueryParamValue(hit_report->population_id, true);
    if (up_str.size() > 512) {
      DCHECK(false) << "population_id is too long: " << up_str;
      up_str = "UP_STRING_TOO_LONG";
    }

    user_population_comp = "&up=" + up_str;
  }

  return GURL(base::StringPrintf(
      "%s&evts=%s&evtd=%s&evtr=%s&evhr=%s&evtb=%d&src=%s&m=%d%s", url.c_str(),
      threat_list.c_str(),
      base::EscapeQueryParamValue(hit_report->malicious_url.spec(), true)
          .c_str(),
      base::EscapeQueryParamValue(hit_report->page_url.spec(), true).c_str(),
      base::EscapeQueryParamValue(hit_report->referrer_url.spec(), true)
          .c_str(),
      hit_report->is_subresource, threat_source.c_str(),
      hit_report->is_metrics_reporting_active, user_population_comp.c_str()));
}

GURL PingManager::ThreatDetailsUrl() const {
  std::string url = GetReportUrl(config_, "clientreport/malware");
  return GURL(url);
}

void PingManager::SanitizeThreatDetailsReport(
    ClientSafeBrowsingReportRequest* report) {
  if (report->has_url()) {
    report->set_url(GetSanitizedUrl(report->url()));
  }
  if (report->has_page_url()) {
    report->set_page_url(GetSanitizedUrl(report->page_url()));
  }
  if (report->has_referrer_url()) {
    report->set_referrer_url(GetSanitizedUrl(report->referrer_url()));
  }
  for (auto& resource : *report->mutable_resources()) {
    if (resource.has_url()) {
      resource.set_url(GetSanitizedUrl(resource.url()));
    }
  }
}

void PingManager::SanitizeHitReport(HitReport* hit_report) {
  hit_report->malicious_url = GetSanitizedUrl(hit_report->malicious_url);
  hit_report->page_url = GetSanitizedUrl(hit_report->page_url);
  hit_report->referrer_url = GetSanitizedUrl(hit_report->referrer_url);
}

void PingManager::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void PingManager::SetTokenFetcherForTesting(
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher) {
  token_fetcher_ = std::move(token_fetcher);
}

base::WeakPtr<PingManager> PingManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
