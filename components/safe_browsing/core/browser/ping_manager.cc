// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/ping_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"
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

using WriteResult = safe_browsing::PingManager::Persister::WriteResult;

// Delay before reading persisted reports at startup.
base::TimeDelta kReadPersistedReportsDelay = base::Seconds(15);

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

bool IsDownloadReport(
    safe_browsing::ClientSafeBrowsingReportRequest::ReportType type) {
  switch (type) {
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_RECOVERY:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_WARNING:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_BY_API:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_OPENED:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_AUTO_DELETED:
    case safe_browsing::ClientSafeBrowsingReportRequest::
        DANGEROUS_DOWNLOAD_PROFILE_CLOSED:
      return true;
    default:
      return false;
  }
}

std::string GetRandFileName() {
  return base::NumberToString(
      base::RandGenerator(std::numeric_limits<uint64_t>::max()));
}

void RecordPersisterWriteResult(WriteResult write_result) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.ClientSafeBrowsingReport.PersisterWriteResult",
      write_result);
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

// SafeBrowsingPingManager::Persister implementation -----------------------

PingManager::Persister::Persister(const base::FilePath& persister_root_path) {
  dir_path_ = persister_root_path.AppendASCII("DownloadReports");
}

void PingManager::Persister::WriteReport(const std::string& serialized_report) {
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(dir_path_, &error)) {
    RecordPersisterWriteResult(WriteResult::kFailedCreateDirectory);
    return;
  }
  base::FilePath file_path = dir_path_.AppendASCII((GetRandFileName()));
  bool success = base::WriteFile(file_path, serialized_report);
  RecordPersisterWriteResult(success ? WriteResult::kSuccess
                                     : WriteResult::kFailedWriteFile);
}

std::vector<std::string> PingManager::Persister::ReadAndDeleteReports() {
  if (!base::DirectoryExists(dir_path_)) {
    return {};
  }
  base::FileEnumerator directory_enumerator(dir_path_,
                                            /*recursive=*/false,
                                            base::FileEnumerator::FILES);
  std::vector<std::string> persisted_reports;
  for (base::FilePath file_name = directory_enumerator.Next();
       !file_name.empty(); file_name = directory_enumerator.Next()) {
    std::string persisted_report;
    bool success = base::ReadFileToString(file_name, &persisted_report);
    base::UmaHistogramBoolean(
        "SafeBrowsing.ClientSafeBrowsingReport.PersisterReadReportSuccessful",
        success);
    if (success) {
      persisted_reports.emplace_back(std::move(persisted_report));
    }
  }
  // Since persisted reports are uncommon, delete the directory so that we don't
  // leave an empty directory going forward.
  base::DeletePathRecursively(dir_path_);
  base::UmaHistogramCounts1000(
      "SafeBrowsing.ClientSafeBrowsingReport.PersisterReportCountOnStartup",
      persisted_reports.size());
  return persisted_reports;
}

// SafeBrowsingPingManager implementation ----------------------------------

// static
std::unique_ptr<PingManager> PingManager::Create(
    const V4ProtocolConfig& config,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    base::RepeatingCallback<bool()> get_should_fetch_access_token,
    WebUIDelegate* webui_delegate,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::RepeatingCallback<ChromeUserPopulation()>
        get_user_population_callback,
    base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>
        get_page_load_token_callback,
    std::unique_ptr<SafeBrowsingHatsDelegate> hats_delegate,
    const base::FilePath& persister_root_path,
    base::RepeatingCallback<bool()> get_should_send_persisted_report) {
  return std::make_unique<PingManager>(
      config, url_loader_factory, std::move(token_fetcher),
      get_should_fetch_access_token, webui_delegate, ui_task_runner,
      get_user_population_callback, get_page_load_token_callback,
      std::move(hats_delegate), persister_root_path,
      std::move(get_should_send_persisted_report));
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
        get_page_load_token_callback,
    std::unique_ptr<SafeBrowsingHatsDelegate> hats_delegate,
    const base::FilePath& persister_root_path,
    base::RepeatingCallback<bool()> get_should_send_persisted_report)
    : config_(config),
      url_loader_factory_(url_loader_factory),
      token_fetcher_(std::move(token_fetcher)),
      get_should_fetch_access_token_(get_should_fetch_access_token),
      webui_delegate_(webui_delegate),
      ui_task_runner_(ui_task_runner),
      get_user_population_callback_(get_user_population_callback),
      get_page_load_token_callback_(get_page_load_token_callback),
      hats_delegate_(std::move(hats_delegate)),
      get_should_send_persisted_report_(
          std::move(get_should_send_persisted_report)) {
  persister_ = base::SequenceBound<Persister>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      persister_root_path);
  // Post this task with a delay to avoid running right at Chrome startup
  // when a lot of other startup tasks are running.
  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PingManager::ReadPersistedReports,
                     weak_factory_.GetWeakPtr()),
      kReadPersistedReportsDelay);
}

PingManager::~PingManager() {}

// All SafeBrowsing request responses are handled here.
void PingManager::OnURLLoaderComplete(
    network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  auto it = safebrowsing_reports_.find(source);
  CHECK(it != safebrowsing_reports_.end(), base::NotFatalUntil::M130);
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
  base::UmaHistogramEnumeration("SafeBrowsing.HitReport.ThreatType",
                                hit_report->threat_type);

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
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  SanitizeThreatDetailsReport(report.get());
  if (!get_user_population_callback_.is_null()) {
    *report->mutable_population() = get_user_population_callback_.Run();
  }
  if (!get_page_load_token_callback_.is_null()) {
    ChromeUserPopulation::PageLoadToken token =
        get_page_load_token_callback_.Run(GURL(report->page_url()));
    report->mutable_population()->mutable_page_load_tokens()->Add()->Swap(
        &token);
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
  if (get_should_fetch_access_token_.Run()) {
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
  if (IsDownloadReport(report->type())) {
    base::UmaHistogramCounts1M(
        "SafeBrowsing.ClientSafeBrowsingReport.DownloadReportSize",
        serialized_report.size());
  }
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

PingManager::PersistThreatDetailsResult
PingManager::PersistThreatDetailsAndReportOnNextStartup(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  SanitizeThreatDetailsReport(report.get());
  std::string serialized_report;
  if (!report->SerializeToString(&serialized_report)) {
    return PersistThreatDetailsResult::kSerializationError;
  }
  if (serialized_report.empty()) {
    return PersistThreatDetailsResult::kEmptyReport;
  }
  persister_.AsyncCall(&Persister::WriteReport)
      .WithArgs(std::move(serialized_report));
  return PersistThreatDetailsResult::kPersistTaskPosted;
}

void PingManager::ReadPersistedReports() {
  persister_.AsyncCall(&PingManager::Persister::ReadAndDeleteReports)
      .Then(base::BindOnce(&PingManager::OnReadPersistedReportsDone,
                           weak_factory_.GetWeakPtr()));
}

void PingManager::OnReadPersistedReportsDone(
    std::vector<std::string> serialized_reports) {
  CHECK(!get_should_send_persisted_report_.is_null());
  if (!get_should_send_persisted_report_.Run()) {
    return;
  }
  for (const std::string& seralized_report : serialized_reports) {
    if (seralized_report.empty()) {
      continue;
    }
    auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
    if (report->ParseFromString(seralized_report)) {
      ReportThreatDetails(std::move(report));
    }
  }
}

void PingManager::AttachThreatDetailsAndLaunchSurvey(
    std::unique_ptr<ClientSafeBrowsingReportRequest> report) {
  // Return early if HaTS survey is disabled by policy.
  if (!hats_delegate_) {
    return;
  }
  static constexpr auto valid_report_types =
      base::MakeFixedFlatSet<ClientSafeBrowsingReportRequest::ReportType>(
          {ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING,
           ClientSafeBrowsingReportRequest::URL_PHISHING,
           ClientSafeBrowsingReportRequest::URL_UNWANTED,
           ClientSafeBrowsingReportRequest::URL_MALWARE});
  CHECK(base::Contains(valid_report_types, report->type()));
  SanitizeThreatDetailsReport(report.get());
  if (!get_user_population_callback_.is_null()) {
    *report->mutable_population() = get_user_population_callback_.Run();
  }
  if (!get_page_load_token_callback_.is_null()) {
    ChromeUserPopulation::PageLoadToken token =
        get_page_load_token_callback_.Run(GURL(report->page_url()));
    report->mutable_population()->mutable_page_load_tokens()->Add()->Swap(
        &token);
  }
  std::string serialized_report;
  if (!report->SerializeToString(&serialized_report)) {
    DLOG(ERROR) << "Unable to serialize the threat report.";
    return;
  }
  if (serialized_report.empty()) {
    DLOG(ERROR) << "The threat report is empty.";
    return;
  }
  std::string url_encoded_serialized_report;
  base::Base64UrlEncode(serialized_report,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &url_encoded_serialized_report);
  hats_delegate_->LaunchRedWarningSurvey(
      {{kFlaggedUrl, report->url()},
       {kMainFrameUrl, report->page_url()},
       {kReferrerUrl, report->referrer_url()},
       {kUserActivityWithUrls, url_encoded_serialized_report}});
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
    LogAuthenticatedCookieResets(
        *resource_request, SafeBrowsingAuthenticatedEndpoint::kThreatDetails);
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
  using enum SBThreatType;

  DCHECK(hit_report->threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_BINARY_MALWARE ||
         hit_report->threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING);
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
    default:
      NOTREACHED_IN_MIGRATION();
  }

  std::string threat_source = "none";
  switch (hit_report->threat_source) {
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      threat_source = "l4";
      break;
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
      threat_source = "csd";
      break;
    case safe_browsing::ThreatSource::URL_REAL_TIME_CHECK:
      threat_source = "rt";
      break;
    case safe_browsing::ThreatSource::NATIVE_PVER5_REAL_TIME:
      threat_source = "n5rt";
      break;
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME:
      threat_source = "asbrt";
      break;
    case safe_browsing::ThreatSource::ANDROID_SAFEBROWSING:
      threat_source = "asb";
      break;
    case safe_browsing::ThreatSource::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
  }

  return GURL(base::StringPrintf(
      "%s&evts=%s&evtd=%s&evtr=%s&evhr=%s&evtb=%d&src=%s&m=%d", url.c_str(),
      threat_list.c_str(),
      base::EscapeQueryParamValue(hit_report->malicious_url.spec(), true)
          .c_str(),
      base::EscapeQueryParamValue(hit_report->page_url.spec(), true).c_str(),
      base::EscapeQueryParamValue(hit_report->referrer_url.spec(), true)
          .c_str(),
      hit_report->is_subresource, threat_source.c_str(),
      hit_report->is_metrics_reporting_active));
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

void PingManager::SetHatsDelegateForTesting(
    std::unique_ptr<SafeBrowsingHatsDelegate> hats_delegate) {
  hats_delegate_ = std::move(hats_delegate);
}

}  // namespace safe_browsing
