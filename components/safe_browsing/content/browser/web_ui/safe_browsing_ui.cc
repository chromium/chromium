// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/grit/components_scaled_resources.h"
#include "components/grit/safe_browsing_resources.h"
#include "components/grit/safe_browsing_resources_map.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/csd.to_value.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.to_value.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.to_value.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/proto/connectors.to_value.h"
#endif

using base::Time;
using sync_pb::GaiaPasswordReuse;
using PasswordCaptured = sync_pb::UserEventSpecifics::GaiaPasswordCaptured;
using PasswordReuseLookup = sync_pb::GaiaPasswordReuse::PasswordReuseLookup;
using PasswordReuseDetected = sync_pb::GaiaPasswordReuse::PasswordReuseDetected;
using PasswordReuseDialogInteraction =
    sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction;

namespace safe_browsing {
WebUIInfoSingleton::WebUIInfoSingleton() = default;

WebUIInfoSingleton::~WebUIInfoSingleton() = default;

// static
WebUIInfoSingleton* WebUIInfoSingleton::GetInstance() {
  CHECK(base::CommandLine::ForCurrentProcess()
            ->GetSwitchValueASCII("type")
            .empty())
      << "chrome://safe-browsing WebUI is only available in the browser "
         "process";
  static base::NoDestructor<WebUIInfoSingleton> instance;
  return instance.get();
}

// static
bool WebUIInfoSingleton::HasListener() {
  return GetInstance()->has_test_listener_ ||
         !GetInstance()->webui_instances_.empty();
}

void WebUIInfoSingleton::AddToDownloadUrlsChecked(const std::vector<GURL>& urls,
                                                  DownloadCheckResult result) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyDownloadUrlCheckedJsListener(urls, result);
  }
  download_urls_checked_.emplace_back(urls, result);
}

void WebUIInfoSingleton::AddToClientDownloadRequestsSent(
    std::unique_ptr<ClientDownloadRequest> client_download_request) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientDownloadRequestJsListener(
        client_download_request.get());
  }
  client_download_requests_sent_.emplace_back(
      std::move(client_download_request));
}

void WebUIInfoSingleton::ClearDownloadUrlsChecked() {
  std::vector<std::pair<std::vector<GURL>, DownloadCheckResult>>().swap(
      download_urls_checked_);
}

void WebUIInfoSingleton::ClearClientDownloadRequestsSent() {
  std::vector<std::unique_ptr<ClientDownloadRequest>>().swap(
      client_download_requests_sent_);
}

void WebUIInfoSingleton::AddToClientDownloadResponsesReceived(
    std::unique_ptr<ClientDownloadResponse> client_download_response) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientDownloadResponseJsListener(
        client_download_response.get());
  }
  client_download_responses_received_.emplace_back(
      std::move(client_download_response));
}

void WebUIInfoSingleton::ClearClientDownloadResponsesReceived() {
  std::vector<std::unique_ptr<ClientDownloadResponse>>().swap(
      client_download_responses_received_);
}

void WebUIInfoSingleton::AddToClientPhishingRequestsSent(
    std::unique_ptr<ClientPhishingRequest> client_phishing_request,
    std::string token) {
  if (!HasListener()) {
    return;
  }
  ClientPhishingRequestAndToken ping(std::move(*client_phishing_request),
                                     std::move(token));
  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientPhishingRequestJsListener(ping);
  }
  client_phishing_requests_sent_.emplace_back(std::move(ping));
}

void WebUIInfoSingleton::ClearClientPhishingRequestsSent() {
  std::vector<ClientPhishingRequestAndToken>().swap(
      client_phishing_requests_sent_);
}

void WebUIInfoSingleton::AddToClientPhishingResponsesReceived(
    std::unique_ptr<ClientPhishingResponse> client_phishing_response) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientPhishingResponseJsListener(
        client_phishing_response.get());
  }
  client_phishing_responses_received_.emplace_back(
      std::move(client_phishing_response));
}

void WebUIInfoSingleton::ClearClientPhishingResponsesReceived() {
  std::vector<std::unique_ptr<ClientPhishingResponse>>().swap(
      client_phishing_responses_received_);
}

void WebUIInfoSingleton::AddToCSBRRsSent(
    std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyCSBRRJsListener(csbrr.get());
  }
  csbrrs_sent_.emplace_back(std::move(csbrr));
  if (on_csbrr_logged_for_testing_) {
    std::move(on_csbrr_logged_for_testing_).Run();
  }
}

void WebUIInfoSingleton::ClearCSBRRsSent() {
  std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>().swap(
      csbrrs_sent_);
}

void WebUIInfoSingleton::SetOnCSBRRLoggedCallbackForTesting(
    base::OnceClosure on_done) {
  on_csbrr_logged_for_testing_ = std::move(on_done);
}

void WebUIInfoSingleton::AddToHitReportsSent(
    std::unique_ptr<HitReport> hit_report) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyHitReportJsListener(hit_report.get());
  }
  hit_reports_sent_.emplace_back(std::move(hit_report));
}

void WebUIInfoSingleton::ClearHitReportsSent() {
  std::vector<std::unique_ptr<HitReport>>().swap(hit_reports_sent_);
}

void WebUIInfoSingleton::AddToPGEvents(
    const sync_pb::UserEventSpecifics& event) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyPGEventJsListener(event);
  }

  pg_event_log_.emplace_back(event);
}

void WebUIInfoSingleton::ClearPGEvents() {
  std::vector<sync_pb::UserEventSpecifics>().swap(pg_event_log_);
}

void WebUIInfoSingleton::AddToSecurityEvents(
    const sync_pb::GaiaPasswordReuse& event) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifySecurityEventJsListener(event);
  }

  security_event_log_.emplace_back(event);
}

void WebUIInfoSingleton::ClearSecurityEvents() {
  std::vector<sync_pb::GaiaPasswordReuse>().swap(security_event_log_);
}

int WebUIInfoSingleton::AddToPGPings(
    const LoginReputationClientRequest& request,
    const std::string& oauth_token) {
  if (!HasListener()) {
    return -1;
  }

  LoginReputationClientRequestAndToken ping(request, oauth_token);

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyPGPingJsListener(pg_pings_.size(), ping);
  }

  pg_pings_.emplace_back(std::move(ping));

  return pg_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToPGResponses(
    int token,
    const LoginReputationClientResponse& response) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyPGResponseJsListener(token, response);
  }

  pg_responses_[token] = response;
}

void WebUIInfoSingleton::ClearPGPings() {
  std::vector<LoginReputationClientRequestAndToken>().swap(pg_pings_);
  std::map<int, LoginReputationClientResponse>().swap(pg_responses_);
}

int WebUIInfoSingleton::AddToURTLookupPings(const RTLookupRequest& request,
                                            const std::string& oauth_token) {
  if (!HasListener()) {
    return -1;
  }

  URTLookupRequest ping(request, oauth_token);

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyURTLookupPingJsListener(urt_lookup_pings_.size(),
                                                  ping);
  }

  urt_lookup_pings_.emplace_back(std::move(ping));

  return urt_lookup_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToURTLookupResponses(
    int token,
    const RTLookupResponse& response) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyURTLookupResponseJsListener(token, response);
  }

  urt_lookup_responses_[token] = response;
}

void WebUIInfoSingleton::ClearURTLookupPings() {
  std::vector<URTLookupRequest>().swap(urt_lookup_pings_);
  std::map<int, RTLookupResponse>().swap(urt_lookup_responses_);
}

std::optional<int> WebUIInfoSingleton::AddToHPRTLookupPings(
    V5::SearchHashesRequest* inner_request,
    std::string relay_url_spec,
    std::string ohttp_key) {
  if (!HasListener()) {
    return std::nullopt;
  }
  HPRTLookupRequest request(*inner_request, std::move(relay_url_spec),
                            std::move(ohttp_key));
  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyHPRTLookupPingJsListener(hprt_lookup_pings_.size(),
                                                   request);
  }
  hprt_lookup_pings_.emplace_back(std::move(request));
  return hprt_lookup_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToHPRTLookupResponses(
    int token,
    V5::SearchHashesResponse* response) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyHPRTLookupResponseJsListener(token, *response);
  }

  hprt_lookup_responses_[token] = *response;
}

void WebUIInfoSingleton::ClearHPRTLookupPings() {
  std::vector<HPRTLookupRequest>().swap(hprt_lookup_pings_);
  std::map<int, V5::SearchHashesResponse>().swap(hprt_lookup_responses_);
}

void WebUIInfoSingleton::LogMessage(const std::string& message) {
  if (!HasListener()) {
    return;
  }

  base::Time timestamp = base::Time::Now();
  log_messages_.push_back(std::make_pair(timestamp, message));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&WebUIInfoSingleton::NotifyLogMessageListeners,
                                timestamp, message));
}

void WebUIInfoSingleton::ClearLogMessages() {
  std::vector<std::pair<base::Time, std::string>>().swap(log_messages_);
}

/* static */ void WebUIInfoSingleton::NotifyLogMessageListeners(
    const base::Time& timestamp,
    const std::string& message) {
  WebUIInfoSingleton* web_ui_info = GetInstance();

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       web_ui_info->webui_instances()) {
    webui_listener->NotifyLogMessageJsListener(timestamp, message);
  }
}

void WebUIInfoSingleton::AddToReportingEvents(const base::Value::Dict& event) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyReportingEventJsListener(event);
  }

  reporting_events_.emplace_back(event.Clone());
}

void WebUIInfoSingleton::ClearReportingEvents() {
  std::vector<base::Value::Dict>().swap(reporting_events_);
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
void WebUIInfoSingleton::AddToDeepScanRequests(
    bool per_profile_request,
    const std::string& access_token,
    const std::string& upload_info,
    const std::string& upload_url,
    const enterprise_connectors::ContentAnalysisRequest& request) {
  if (!HasListener()) {
    return;
  }

  // Only update the request time the first time we see a token.
  if (deep_scan_requests_.find(request.request_token()) ==
      deep_scan_requests_.end()) {
    deep_scan_requests_[request.request_token()].request_time =
        base::Time::Now();
  }

  auto& deep_scan_request = deep_scan_requests_[request.request_token()];
  deep_scan_request.per_profile_request = per_profile_request;
  deep_scan_request.request = request;

  if (access_token.empty()) {
    deep_scan_request.access_token_truncated = "NONE";
  } else {
    // Only show the first few bytes of `access_token` as it's sensitive.
    deep_scan_request.access_token_truncated =
        base::StrCat({access_token.substr(0, std::min(access_token.size(),
                                                      static_cast<size_t>(6))),
                      "..."});
  }

  deep_scan_request.upload_info = upload_info;
  deep_scan_request.upload_url = upload_url;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyDeepScanJsListener(
        request.request_token(), deep_scan_requests_[request.request_token()]);
  }
}

void WebUIInfoSingleton::AddToDeepScanResponses(
    const std::string& token,
    const std::string& status,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  if (!HasListener()) {
    return;
  }

  deep_scan_requests_[token].response_time = base::Time::Now();
  deep_scan_requests_[token].response_status = status;
  deep_scan_requests_[token].response = response;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyDeepScanJsListener(token, deep_scan_requests_[token]);
  }
}

void WebUIInfoSingleton::ClearDeepScans() {
  base::flat_map<std::string, DeepScanDebugData>().swap(deep_scan_requests_);
}

void WebUIInfoSingleton::SetTailoredVerdictOverride(
    ClientDownloadResponse::TailoredVerdict new_value,
    const SafeBrowsingUIHandler* new_source) {
  tailored_verdict_override_.Set(std::move(new_value), new_source);

  // Notify other listeners of the change. The source itself is notified by the
  // caller.
  for (SafeBrowsingUIHandler* listener : webui_instances()) {
    if (!tailored_verdict_override_.IsFromSource(listener)) {
      listener->NotifyTailoredVerdictOverrideJsListener();
    }
  }
}

void WebUIInfoSingleton::ClearTailoredVerdictOverride() {
  tailored_verdict_override_.Clear();

  // Notify other listeners of the change. The source itself is notified by the
  // caller.
  for (SafeBrowsingUIHandler* listener : webui_instances()) {
    if (!tailored_verdict_override_.IsFromSource(listener)) {
      listener->NotifyTailoredVerdictOverrideJsListener();
    }
  }
}
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

void WebUIInfoSingleton::RegisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  webui_instances_.push_back(webui);
}

void WebUIInfoSingleton::UnregisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  std::erase(webui_instances_, webui);

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  // Notify other WebUIs that the source of the tailored verdict override is
  // going away.
  if (tailored_verdict_override_.IsFromSource(webui)) {
    tailored_verdict_override_.Clear();
    for (SafeBrowsingUIHandler* listener : webui_instances()) {
      listener->NotifyTailoredVerdictOverrideJsListener();
    }
  }
#endif

  MaybeClearData();
}

mojo::Remote<network::mojom::CookieManager>
WebUIInfoSingleton::GetCookieManager(content::BrowserContext* browser_context) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote;
  if (sb_service_) {
    sb_service_->GetNetworkContext(browser_context)
        ->GetCookieManager(cookie_manager_remote.BindNewPipeAndPassReceiver());
  }

  return cookie_manager_remote;
}

ReferrerChainProvider* WebUIInfoSingleton::GetReferrerChainProvider(
    content::BrowserContext* browser_context) {
  if (!sb_service_) {
    return nullptr;
  }

  return sb_service_->GetReferrerChainProviderFromBrowserContext(
      browser_context);
}

#if BUILDFLAG(IS_ANDROID)
internal::ReferringAppInfo WebUIInfoSingleton::GetReferringAppInfo(
    content::WebContents* web_contents) {
  return sb_service_ ? sb_service_->GetReferringAppInfo(web_contents)
                     : internal::ReferringAppInfo{};
}
#endif

void WebUIInfoSingleton::ClearListenerForTesting() {
  has_test_listener_ = false;
  on_csbrr_logged_for_testing_ = base::NullCallback();
  MaybeClearData();
}

void WebUIInfoSingleton::MaybeClearData() {
  if (!HasListener()) {
    ClearCSBRRsSent();
    ClearHitReportsSent();
    ClearDownloadUrlsChecked();
    ClearClientDownloadRequestsSent();
    ClearClientDownloadResponsesReceived();
    ClearClientPhishingRequestsSent();
    ClearClientPhishingResponsesReceived();
    ClearPGEvents();
    ClearPGPings();
    ClearURTLookupPings();
    ClearHPRTLookupPings();
    ClearLogMessages();
    ClearReportingEvents();

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
    ClearDeepScans();
    ClearTailoredVerdictOverride();
#endif
  }
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
DeepScanDebugData::DeepScanDebugData() = default;
DeepScanDebugData::DeepScanDebugData(const DeepScanDebugData&) = default;
DeepScanDebugData::~DeepScanDebugData() = default;

TailoredVerdictOverrideData::TailoredVerdictOverrideData() = default;
TailoredVerdictOverrideData::~TailoredVerdictOverrideData() = default;

void TailoredVerdictOverrideData::Set(
    ClientDownloadResponse::TailoredVerdict new_value,
    const SafeBrowsingUIHandler* new_source) {
  override_value = std::move(new_value);
  source = reinterpret_cast<SourceId>(new_source);
}

bool TailoredVerdictOverrideData::IsFromSource(
    const SafeBrowsingUIHandler* maybe_source) const {
  return reinterpret_cast<SourceId>(maybe_source) == source;
}

void TailoredVerdictOverrideData::Clear() {
  override_value.reset();
  source = 0u;
}
#endif

namespace {

std::string SerializeJson(base::ValueView value) {
  return base::WriteJsonWithOptions(value,
                                    base::JSONWriter::OPTIONS_PRETTY_PRINT)
      .value_or(std::string());
}

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

std::string UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds) {
  base::Time time =
      base::Time::UnixEpoch() + base::Milliseconds(time_in_milliseconds);
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

void AddStoreInfo(
    const DatabaseManagerInfo::DatabaseInfo::StoreInfo& store_info,
    base::Value::List& database_info_list) {
  if (store_info.has_file_name()) {
    database_info_list.Append(store_info.file_name());
  } else {
    database_info_list.Append("Unknown store");
  }

  base::Value::List store_info_list;
  if (store_info.has_file_size_bytes()) {
    store_info_list.Append(
        "Size (in bytes): " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.file_size_bytes())));
  }

  if (store_info.has_update_status()) {
    store_info_list.Append(
        "Update status: " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.update_status())));
  }

  if (store_info.has_last_apply_update_time_millis()) {
    store_info_list.Append("Last update time: " +
                           UserReadableTimeFromMillisSinceEpoch(
                               store_info.last_apply_update_time_millis()));
  }

  if (store_info.has_checks_attempted()) {
    store_info_list.Append(
        "Number of database checks: " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.checks_attempted())));
  }

  if (store_info.has_state()) {
    std::string state_base64 = base::Base64Encode(store_info.state());
    store_info_list.Append("State: " + state_base64);
  }

  for (const auto& prefix_set : store_info.prefix_sets()) {
    std::string size = base::UTF16ToUTF8(base::FormatNumber(prefix_set.size()));
    std::string count =
        base::UTF16ToUTF8(base::FormatNumber(prefix_set.count()));
    store_info_list.Append(count + " prefixes of size " + size);
  }

  database_info_list.Append(std::move(store_info_list));
}

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo& database_info,
                     base::Value::List& database_info_list) {
  if (database_info.has_database_size_bytes()) {
    database_info_list.Append("Database size (in bytes)");
    database_info_list.Append(
        static_cast<double>(database_info.database_size_bytes()));
  }

  // Add the information specific to each store.
  for (int i = 0; i < database_info.store_info_size(); i++) {
    AddStoreInfo(database_info.store_info(i), database_info_list);
  }
}

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo& update_info,
                   base::Value::List& database_info_list) {
  if (update_info.has_network_status_code()) {
    // Network status of the last GetUpdate().
    database_info_list.Append("Last update network status code");
    database_info_list.Append(update_info.network_status_code());
  }
  if (update_info.has_last_update_time_millis()) {
    database_info_list.Append("Last update time");
    database_info_list.Append(UserReadableTimeFromMillisSinceEpoch(
        update_info.last_update_time_millis()));
  }
  if (update_info.has_next_update_time_millis()) {
    database_info_list.Append("Next update time");
    database_info_list.Append(UserReadableTimeFromMillisSinceEpoch(
        update_info.next_update_time_millis()));
  }
}

void ParseFullHashInfo(
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo&
        full_hash_info,
    base::Value::Dict& full_hash_info_dict) {
  if (full_hash_info.has_positive_expiry()) {
    full_hash_info_dict.Set(
        "Positive expiry",
        UserReadableTimeFromMillisSinceEpoch(full_hash_info.positive_expiry()));
  }
  if (full_hash_info.has_full_hash()) {
    std::string full_hash;
    base::Base64UrlEncode(full_hash_info.full_hash(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &full_hash);
    full_hash_info_dict.Set("Full hash (base64)", std::move(full_hash));
  }
  if (full_hash_info.list_identifier().has_platform_type()) {
    full_hash_info_dict.Set("platform_type",
                            full_hash_info.list_identifier().platform_type());
  }
  if (full_hash_info.list_identifier().has_threat_entry_type()) {
    full_hash_info_dict.Set(
        "threat_entry_type",
        full_hash_info.list_identifier().threat_entry_type());
  }
  if (full_hash_info.list_identifier().has_threat_type()) {
    full_hash_info_dict.Set("threat_type",
                            full_hash_info.list_identifier().threat_type());
  }
}

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache& full_hash_cache,
                        base::Value::List& full_hash_cache_list) {
  base::Value::Dict full_hash_cache_parsed;

  if (full_hash_cache.has_hash_prefix()) {
    std::string hash_prefix;
    base::Base64UrlEncode(full_hash_cache.hash_prefix(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &hash_prefix);
    full_hash_cache_parsed.Set("Hash prefix (base64)", std::move(hash_prefix));
  }
  if (full_hash_cache.cached_hash_prefix_info().has_negative_expiry()) {
    full_hash_cache_parsed.Set(
        "Negative expiry",
        UserReadableTimeFromMillisSinceEpoch(
            full_hash_cache.cached_hash_prefix_info().negative_expiry()));
  }

  full_hash_cache_list.Append(std::move(full_hash_cache_parsed));

  for (const auto& full_hash_info_it :
       full_hash_cache.cached_hash_prefix_info().full_hash_info()) {
    base::Value::Dict full_hash_info_dict;
    ParseFullHashInfo(full_hash_info_it, full_hash_info_dict);
    full_hash_cache_list.Append(std::move(full_hash_info_dict));
  }
}

void ParseFullHashCacheInfo(const FullHashCacheInfo& full_hash_cache_info_proto,
                            base::Value::List& full_hash_cache_info) {
  if (full_hash_cache_info_proto.has_number_of_hits()) {
    base::Value::Dict number_of_hits;
    number_of_hits.Set("Number of cache hits",
                       full_hash_cache_info_proto.number_of_hits());
    full_hash_cache_info.Append(std::move(number_of_hits));
  }

  // Record FullHashCache list.
  for (const auto& full_hash_cache_it :
       full_hash_cache_info_proto.full_hash_cache()) {
    base::Value::List full_hash_cache_list;
    ParseFullHashCache(full_hash_cache_it, full_hash_cache_list);
    full_hash_cache_info.Append(std::move(full_hash_cache_list));
  }
}

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo& full_hash_cache_info_proto) {
  base::Value::List full_hash_cache;
  ParseFullHashCacheInfo(full_hash_cache_info_proto, full_hash_cache);
  return SerializeJson(full_hash_cache);
}

#endif

std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr) {
  return SerializeJson(proto_to_value::Serialize(cdr));
}

std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr) {
  return SerializeJson(proto_to_value::Serialize(cdr));
}

std::string SerializeClientPhishingRequest(
    const ClientPhishingRequestAndToken& cprat) {
  base::Value::Dict value = proto_to_value::Serialize(cprat.request);
  value.Set("scoped_oauthtoken", cprat.token);
  return SerializeJson(std::move(value));
}

std::string SerializeClientPhishingResponse(const ClientPhishingResponse& cpr) {
  return SerializeJson(proto_to_value::Serialize(cpr));
}

std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report) {
  return SerializeJson(proto_to_value::Serialize(report));
}

std::string SerializeHitReport(const HitReport& hit_report) {
  base::Value::Dict hit_report_dict;
  hit_report_dict.Set("malicious_url", hit_report.malicious_url.spec());
  hit_report_dict.Set("page_url", hit_report.page_url.spec());
  hit_report_dict.Set("referrer_url", hit_report.referrer_url.spec());
  hit_report_dict.Set("is_subresource", hit_report.is_subresource);
  std::string threat_type;
  switch (hit_report.threat_type) {
    case SBThreatType::SB_THREAT_TYPE_URL_PHISHING:
      threat_type = "SB_THREAT_TYPE_URL_PHISHING";
      break;
    case SBThreatType::SB_THREAT_TYPE_URL_MALWARE:
      threat_type = "SB_THREAT_TYPE_URL_MALWARE";
      break;
    case SBThreatType::SB_THREAT_TYPE_URL_UNWANTED:
      threat_type = "SB_THREAT_TYPE_URL_UNWANTED";
      break;
    case SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE:
      threat_type = "SB_THREAT_TYPE_URL_BINARY_MALWARE";
      break;
    default:
      threat_type = "OTHER";
  }
  hit_report_dict.Set("threat_type", std::move(threat_type));
  std::string threat_source;
  switch (hit_report.threat_source) {
    case ThreatSource::LOCAL_PVER4:
      threat_source = "LOCAL_PVER4";
      break;
    case ThreatSource::CLIENT_SIDE_DETECTION:
      threat_source = "CLIENT_SIDE_DETECTION";
      break;
    case ThreatSource::URL_REAL_TIME_CHECK:
      threat_source = "URL_REAL_TIME_CHECK";
      break;
    case ThreatSource::NATIVE_PVER5_REAL_TIME:
      threat_source = "NATIVE_PVER5_REAL_TIME";
      break;
    case ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME:
      threat_source = "ANDROID_SAFEBROWSING_REAL_TIME";
      break;
    case ThreatSource::ANDROID_SAFEBROWSING:
      threat_source = "ANDROID_SAFEBROWSING";
      break;
    case ThreatSource::UNKNOWN:
      threat_source = "UNKNOWN";
      break;
  }
  hit_report_dict.Set("threat_source", std::move(threat_source));
  std::string extended_reporting_level;
  switch (hit_report.extended_reporting_level) {
    case ExtendedReportingLevel::SBER_LEVEL_OFF:
      extended_reporting_level = "SBER_LEVEL_OFF";
      break;
    case ExtendedReportingLevel::SBER_LEVEL_LEGACY:
      extended_reporting_level = "SBER_LEVEL_LEGACY";
      break;
    case ExtendedReportingLevel::SBER_LEVEL_SCOUT:
      extended_reporting_level = "SBER_LEVEL_SCOUT";
      break;
    case ExtendedReportingLevel::SBER_LEVEL_ENHANCED_PROTECTION:
      extended_reporting_level = "SBER_LEVEL_ENHANCED_PROTECTION";
      break;
  }
  hit_report_dict.Set("extended_reporting_level",
                      std::move(extended_reporting_level));
  hit_report_dict.Set("is_enhanced_protection",
                      hit_report.is_enhanced_protection);
  hit_report_dict.Set("is_metrics_reporting_active",
                      hit_report.is_metrics_reporting_active);
  hit_report_dict.Set("post_data", hit_report.post_data);
  return SerializeJson(hit_report_dict);
}

base::Value::Dict SerializePGEvent(const sync_pb::UserEventSpecifics& event) {
  base::Value::Dict result;

  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(event.event_time_usec()));
  result.Set("time", timestamp.InMillisecondsFSinceUnixEpoch());

  base::Value::Dict event_dict;

  // Nominally only one of the following if() statements would be true.
  // Note that top-level path is either password_captured, or one of the fields
  // under GaiaPasswordReuse (ie. we've flattened the namespace for simplicity).

  if (event.has_gaia_password_captured_event()) {
    event_dict.SetByDottedPath(
        "password_captured.event_trigger",
        sync_pb::UserEventSpecifics_GaiaPasswordCaptured_EventTrigger_Name(
            event.gaia_password_captured_event().event_trigger()));
  }

  GaiaPasswordReuse reuse = event.gaia_password_reuse_event();
  if (reuse.has_reuse_detected()) {
    event_dict.SetByDottedPath("reuse_detected.status.enabled",
                               reuse.reuse_detected().status().enabled());
    event_dict.SetByDottedPath(
        "reuse_detected.status.reporting_population",
        GaiaPasswordReuse_PasswordReuseDetected_SafeBrowsingStatus_ReportingPopulation_Name(
            reuse.reuse_detected()
                .status()
                .safe_browsing_reporting_population()));
  }

  if (reuse.has_reuse_lookup()) {
    event_dict.SetByDottedPath(
        "reuse_lookup.lookup_result",
        GaiaPasswordReuse_PasswordReuseLookup_LookupResult_Name(
            reuse.reuse_lookup().lookup_result()));
    event_dict.SetByDottedPath(
        "reuse_lookup.verdict",
        GaiaPasswordReuse_PasswordReuseLookup_ReputationVerdict_Name(
            reuse.reuse_lookup().verdict()));
    event_dict.SetByDottedPath("reuse_lookup.verdict_token",
                               reuse.reuse_lookup().verdict_token());
  }

  if (reuse.has_dialog_interaction()) {
    event_dict.SetByDottedPath(
        "dialog_interaction.interaction_result",
        GaiaPasswordReuse_PasswordReuseDialogInteraction_InteractionResult_Name(
            reuse.dialog_interaction().interaction_result()));
  }

  result.Set("message", SerializeJson(event_dict));
  return result;
}

base::Value::Dict SerializeSecurityEvent(
    const sync_pb::GaiaPasswordReuse& event) {
  base::Value::Dict result;

  base::Value::Dict event_dict;
  if (event.has_reuse_lookup()) {
    event_dict.SetByDottedPath(
        "reuse_lookup.lookup_result",
        GaiaPasswordReuse_PasswordReuseLookup_LookupResult_Name(
            event.reuse_lookup().lookup_result()));
    event_dict.SetByDottedPath(
        "reuse_lookup.verdict",
        GaiaPasswordReuse_PasswordReuseLookup_ReputationVerdict_Name(
            event.reuse_lookup().verdict()));
    event_dict.SetByDottedPath("reuse_lookup.verdict_token",
                               event.reuse_lookup().verdict_token());
  }

  result.Set("message", SerializeJson(event_dict));
  return result;
}

#if BUILDFLAG(IS_ANDROID)
// This serializes the internal::ReferringAppInfo struct (not to be confused
// with the protobuf message ReferringAppInfo), which contains intermediate
// information obtained from Java.
base::Value::Dict SerializeReferringAppInfo(
    const internal::ReferringAppInfo& info) {
  base::Value::Dict dict;
  dict.Set("referring_app_source",
           ReferringAppInfo_ReferringAppSource_Name(info.referring_app_source));
  dict.Set("referring_app_info", info.referring_app_name);
  dict.Set("target_url", info.target_url.spec());
  // Do not bother serializing referring_webapk_* here, because they are only
  // populated for a WebAPK, and it is not possible to launch
  // chrome://safe-browsing in a WebAPK, so they will never show up here.
  return dict;
}
#endif

std::string SerializePGPing(
    const LoginReputationClientRequestAndToken& request_and_token) {
  base::Value::Dict request_dict =
      proto_to_value::Serialize(request_and_token.request);
  request_dict.Set("scoped_oauth_token", request_and_token.token);
  return SerializeJson(request_dict);
}

std::string SerializePGResponse(const LoginReputationClientResponse& response) {
  return SerializeJson(proto_to_value::Serialize(response));
}

std::string SerializeURTLookupPing(const URTLookupRequest& ping) {
  base::Value::Dict request_dict = proto_to_value::Serialize(ping.request);
  request_dict.Set("scoped_oauth_token", ping.token);
  return SerializeJson(request_dict);
}

std::string SerializeURTLookupResponse(const RTLookupResponse& response) {
  return SerializeJson(proto_to_value::Serialize(response));
}

std::string SerializeHPRTLookupPing(const HPRTLookupRequest& ping) {
  base::Value::Dict request_dict;

  base::Value::Dict inner_request_dict;
  base::Value::List encoded_hash_prefixes;
  for (const auto& hash_prefix : ping.inner_request.hash_prefixes()) {
    std::string encoded_hash_prefix;
    base::Base64UrlEncode(hash_prefix,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded_hash_prefix);
    encoded_hash_prefixes.Append(std::move(encoded_hash_prefix));
  }
  inner_request_dict.Set("hash_prefixes (base64)",
                         std::move(encoded_hash_prefixes));

  request_dict.Set("inner_request", std::move(inner_request_dict));
  request_dict.Set("relay_url", ping.relay_url_spec);
  std::string encoded_ohttp_key;
  base::Base64UrlEncode(ping.ohttp_key,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_ohttp_key);
  request_dict.Set("ohttp_public_key (base64)", std::move(encoded_ohttp_key));

  return SerializeJson(request_dict);
}

std::string SerializeHPRTLookupResponse(
    const V5::SearchHashesResponse& response) {
  return SerializeJson(proto_to_value::Serialize(response));
}

base::Value::Dict SerializeLogMessage(base::Time timestamp,
                                      const std::string& message) {
  base::Value::Dict result;
  result.Set("time", timestamp.InMillisecondsFSinceUnixEpoch());
  result.Set("message", message);
  return result;
}

base::Value::Dict SerializeReportingEvent(const base::Value::Dict& event) {
  base::Value::Dict result;
  result.Set("message", SerializeJson(event));
  return result;
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
std::string SerializeContentAnalysisRequest(
    bool per_profile_request,
    const std::string& access_token_truncated,
    const std::string& upload_info,
    const std::string& upload_url,
    const enterprise_connectors::ContentAnalysisRequest& request) {
  base::Value::Dict request_dict = proto_to_value::Serialize(request);
  request_dict.Set("access_token", access_token_truncated);
  request_dict.Set("upload_info", upload_info);
  request_dict.Set("upload_url", upload_url);
  return SerializeJson(request_dict);
}

std::string SerializeContentAnalysisResponse(
    const enterprise_connectors::ContentAnalysisResponse& response) {
  return SerializeJson(proto_to_value::Serialize(response));
}

base::Value::Dict SerializeDeepScanDebugData(const std::string& token,
                                             const DeepScanDebugData& data) {
  base::Value::Dict value;
  value.Set("token", token);

  if (!data.request_time.is_null()) {
    value.Set("request_time",
              data.request_time.InMillisecondsFSinceUnixEpoch());
  }

  if (data.request.has_value()) {
    value.Set("request",
              SerializeContentAnalysisRequest(
                  data.per_profile_request, data.access_token_truncated,
                  data.upload_info, data.upload_url, data.request.value()));
  }

  if (!data.response_time.is_null()) {
    value.Set("response_time",
              data.response_time.InMillisecondsFSinceUnixEpoch());
  }

  if (!data.response_status.empty()) {
    value.Set("response_status", data.response_status);
  }

  if (data.response.has_value()) {
    value.Set("response",
              SerializeContentAnalysisResponse(data.response.value()));
  }

  return value;
}
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

}  // namespace

SafeBrowsingUI::SafeBrowsingUI(
    content::WebUI* web_ui,
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate)
    : content::WebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  // Set up the chrome://safe-browsing source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, safe_browsing::kChromeUISafeBrowsingHost);

  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  web_ui->AddMessageHandler(std::make_unique<SafeBrowsingUIHandler>(
      browser_context, std::move(delegate)));

  // Add required resources.
  html_source->AddResourcePaths(kSafeBrowsingResources);
  html_source->AddResourcePath("", IDR_SAFE_BROWSING_SAFE_BROWSING_HTML);

  // Static types
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

SafeBrowsingUI::~SafeBrowsingUI() = default;

SafeBrowsingUIHandler::SafeBrowsingUIHandler(
    content::BrowserContext* context,
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate)
    : browser_context_(context), delegate_(std::move(delegate)) {}

SafeBrowsingUIHandler::~SafeBrowsingUIHandler() {
  WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::OnJavascriptAllowed() {
  // We don't want to register the SafeBrowsingUIHandler with the
  // WebUIInfoSingleton at construction, since this can lead to
  // messages being sent to the renderer before it's ready. So register it here.
  WebUIInfoSingleton::GetInstance()->RegisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::OnJavascriptDisallowed() {
  // In certain situations, Javascript can become disallowed before the
  // destructor is called (e.g. tab refresh/renderer crash). In these situation,
  // we want to stop receiving JS messages.
  WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::GetExperiments(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, GetFeatureStatusList());
}

void SafeBrowsingUIHandler::GetPrefs(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id,
                            safe_browsing::GetSafeBrowsingPreferencesList(
                                user_prefs::UserPrefs::Get(browser_context_)));
}

void SafeBrowsingUIHandler::GetPolicies(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id,
                            safe_browsing::GetSafeBrowsingPoliciesList(
                                user_prefs::UserPrefs::Get(browser_context_)));
}

void SafeBrowsingUIHandler::GetCookie(const base::Value::List& args) {
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();

  cookie_manager_remote_ =
      WebUIInfoSingleton::GetInstance()->GetCookieManager(browser_context_);
  cookie_manager_remote_->GetAllCookies(
      base::BindOnce(&SafeBrowsingUIHandler::OnGetCookie,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void SafeBrowsingUIHandler::OnGetCookie(
    const std::string& callback_id,
    const std::vector<net::CanonicalCookie>& cookies) {
  DCHECK_GE(1u, cookies.size());

  std::string cookie = "No cookie";
  double time = 0.0;
  if (!cookies.empty()) {
    cookie = cookies[0].Value();
    time = cookies[0].CreationDate().InMillisecondsFSinceUnixEpoch();
  }

  base::Value::List response;
  response.Append(std::move(cookie));
  response.Append(time);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, response);
}

void SafeBrowsingUIHandler::GetSavedPasswords(const base::Value::List& args) {
  password_manager::HashPasswordManager hash_manager;
  hash_manager.set_prefs(user_prefs::UserPrefs::Get(browser_context_));
  hash_manager.set_local_prefs(delegate_->GetLocalState());

  base::Value::List saved_passwords;
  for (const password_manager::PasswordHashData& hash_data :
       hash_manager.RetrieveAllPasswordHashes()) {
    saved_passwords.Append(hash_data.username);
    saved_passwords.Append(hash_data.is_gaia_password);
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, saved_passwords);
}

void SafeBrowsingUIHandler::GetDatabaseManagerInfo(
    const base::Value::List& args) {
  base::Value::List database_manager_info;

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
  const V4LocalDatabaseManager* local_database_manager_instance =
      V4LocalDatabaseManager::current_local_database_manager();
  if (local_database_manager_instance) {
    DatabaseManagerInfo database_manager_info_proto;
    FullHashCacheInfo full_hash_cache_info_proto;

    local_database_manager_instance->CollectDatabaseManagerInfo(
        &database_manager_info_proto, &full_hash_cache_info_proto);

    if (database_manager_info_proto.has_update_info()) {
      AddUpdateInfo(database_manager_info_proto.update_info(),
                    database_manager_info);
    }
    if (database_manager_info_proto.has_database_info()) {
      AddDatabaseInfo(database_manager_info_proto.database_info(),
                      database_manager_info);
    }

    database_manager_info.Append(
        AddFullHashCacheInfo(full_hash_cache_info_proto));
  }
#endif

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();

  ResolveJavascriptCallback(callback_id, database_manager_info);
}

std::string SerializeDownloadUrlChecked(const std::vector<GURL>& urls,
                                        DownloadCheckResult result) {
  base::Value::Dict url_and_result;
  base::Value::List urls_value;
  for (const GURL& url : urls) {
    urls_value.Append(url.spec());
  }
  url_and_result.Set("download_url_chain", std::move(urls_value));
  url_and_result.Set("result", DownloadCheckResultToString(result));

  return SerializeJson(url_and_result);
}

void SafeBrowsingUIHandler::GetDownloadUrlsChecked(
    const base::Value::List& args) {
  const std::vector<std::pair<std::vector<GURL>, DownloadCheckResult>>&
      urls_checked = WebUIInfoSingleton::GetInstance()->download_urls_checked();

  base::Value::List urls_checked_value;
  for (const auto& url_and_result : urls_checked) {
    const std::vector<GURL>& urls = url_and_result.first;
    DownloadCheckResult result = url_and_result.second;
    urls_checked_value.Append(SerializeDownloadUrlChecked(urls, result));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, urls_checked_value);
}

void SafeBrowsingUIHandler::GetSentClientDownloadRequests(
    const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientDownloadRequest>>& cdrs =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  base::Value::List cdrs_sent;

  for (const auto& cdr : cdrs) {
    cdrs_sent.Append(SerializeClientDownloadRequest(*cdr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cdrs_sent);
}

void SafeBrowsingUIHandler::GetReceivedClientDownloadResponses(
    const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientDownloadResponse>>& cdrs =
      WebUIInfoSingleton::GetInstance()->client_download_responses_received();

  base::Value::List cdrs_received;

  for (const auto& cdr : cdrs) {
    cdrs_received.Append(SerializeClientDownloadResponse(*cdr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cdrs_received);
}

void SafeBrowsingUIHandler::GetSentClientPhishingRequests(
    const base::Value::List& args) {
  const std::vector<ClientPhishingRequestAndToken>& cprs =
      WebUIInfoSingleton::GetInstance()->client_phishing_requests_sent();

  base::Value::List cprs_sent;

  for (const auto& cpr : cprs) {
    cprs_sent.Append(SerializeClientPhishingRequest(cpr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cprs_sent);
}

void SafeBrowsingUIHandler::GetReceivedClientPhishingResponses(
    const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientPhishingResponse>>& cprs =
      WebUIInfoSingleton::GetInstance()->client_phishing_responses_received();

  base::Value::List cprs_received;

  for (const auto& cpr : cprs) {
    cprs_received.Append(SerializeClientPhishingResponse(*cpr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cprs_received);
}

void SafeBrowsingUIHandler::GetSentCSBRRs(const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>& reports =
      WebUIInfoSingleton::GetInstance()->csbrrs_sent();

  base::Value::List sent_reports;

  for (const auto& report : reports) {
    sent_reports.Append(SerializeCSBRR(*report));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, sent_reports);
}

void SafeBrowsingUIHandler::GetSentHitReports(const base::Value::List& args) {
  const std::vector<std::unique_ptr<HitReport>>& reports =
      WebUIInfoSingleton::GetInstance()->hit_reports_sent();

  base::Value::List sent_reports;

  for (const auto& report : reports) {
    sent_reports.Append(SerializeHitReport(*report));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, sent_reports);
}

void SafeBrowsingUIHandler::GetPGEvents(const base::Value::List& args) {
  const std::vector<sync_pb::UserEventSpecifics>& events =
      WebUIInfoSingleton::GetInstance()->pg_event_log();

  base::Value::List events_sent;

  for (const sync_pb::UserEventSpecifics& event : events) {
    events_sent.Append(SerializePGEvent(event));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, events_sent);
}

void SafeBrowsingUIHandler::GetSecurityEvents(const base::Value::List& args) {
  const std::vector<sync_pb::GaiaPasswordReuse>& events =
      WebUIInfoSingleton::GetInstance()->security_event_log();

  base::Value::List events_sent;

  for (const sync_pb::GaiaPasswordReuse& event : events) {
    events_sent.Append(SerializeSecurityEvent(event));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, events_sent);
}

void SafeBrowsingUIHandler::GetPGPings(const base::Value::List& args) {
  const std::vector<LoginReputationClientRequestAndToken> requests =
      WebUIInfoSingleton::GetInstance()->pg_pings();

  base::Value::List pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::Value::List ping_entry;
    ping_entry.Append(int(request_index));
    ping_entry.Append(SerializePGPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetPGResponses(const base::Value::List& args) {
  const std::map<int, LoginReputationClientResponse> responses =
      WebUIInfoSingleton::GetInstance()->pg_responses();

  base::Value::List responses_sent;
  for (const auto& token_and_response : responses) {
    base::Value::List response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(SerializePGResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetURTLookupPings(const base::Value::List& args) {
  const std::vector<URTLookupRequest> requests =
      WebUIInfoSingleton::GetInstance()->urt_lookup_pings();

  base::Value::List pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::Value::List ping_entry;
    ping_entry.Append(static_cast<int>(request_index));
    ping_entry.Append(SerializeURTLookupPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetURTLookupResponses(
    const base::Value::List& args) {
  const std::map<int, RTLookupResponse> responses =
      WebUIInfoSingleton::GetInstance()->urt_lookup_responses();

  base::Value::List responses_sent;
  for (const auto& token_and_response : responses) {
    base::Value::List response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        SerializeURTLookupResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetHPRTLookupPings(const base::Value::List& args) {
  const std::vector<HPRTLookupRequest> requests =
      WebUIInfoSingleton::GetInstance()->hprt_lookup_pings();

  base::Value::List pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::Value::List ping_entry;
    ping_entry.Append(static_cast<int>(request_index));
    ping_entry.Append(SerializeHPRTLookupPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetHPRTLookupResponses(
    const base::Value::List& args) {
  const std::map<int, V5::SearchHashesResponse> responses =
      WebUIInfoSingleton::GetInstance()->hprt_lookup_responses();

  base::Value::List responses_sent;
  for (const auto& token_and_response : responses) {
    base::Value::List response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        SerializeHPRTLookupResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetReferrerChain(const base::Value::List& args) {
  DCHECK_GE(args.size(), 2U);
  const std::string& url_string = args[1].GetString();

  ReferrerChainProvider* provider =
      WebUIInfoSingleton::GetInstance()->GetReferrerChainProvider(
          browser_context_);

  const std::string& callback_id = args[0].GetString();

  if (!provider) {
    AllowJavascript();
    ResolveJavascriptCallback(callback_id, "");
    return;
  }

  ReferrerChain referrer_chain;
  provider->IdentifyReferrerChainByEventURL(
      GURL(url_string), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 2, &referrer_chain);

  base::Value::List referrer_list;
  for (const ReferrerChainEntry& entry : referrer_chain) {
    referrer_list.Append(proto_to_value::Serialize(entry));
  }

  std::string referrer_chain_serialized = SerializeJson(referrer_list);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, referrer_chain_serialized);
}

#if BUILDFLAG(IS_ANDROID)
void SafeBrowsingUIHandler::GetReferringAppInfo(const base::Value::List& args) {
  base::Value::Dict referring_app_value;
  internal::ReferringAppInfo info =
      WebUIInfoSingleton::GetInstance()->GetReferringAppInfo(
          web_ui()->GetWebContents());
  referring_app_value = SerializeReferringAppInfo(info);

  std::string referring_app_serialized = SerializeJson(referring_app_value);

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, referring_app_serialized);
}
#endif

void SafeBrowsingUIHandler::GetReportingEvents(const base::Value::List& args) {
  base::Value::List reporting_events;
  for (const auto& reporting_event :
       WebUIInfoSingleton::GetInstance()->reporting_events()) {
    reporting_events.Append(reporting_event.Clone());
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, reporting_events);
}

void SafeBrowsingUIHandler::GetLogMessages(const base::Value::List& args) {
  const std::vector<std::pair<base::Time, std::string>>& log_messages =
      WebUIInfoSingleton::GetInstance()->log_messages();

  base::Value::List messages_received;
  for (const auto& message : log_messages) {
    messages_received.Append(
        SerializeLogMessage(message.first, message.second));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, messages_received);
}

void SafeBrowsingUIHandler::GetDeepScans(const base::Value::List& args) {
  base::Value::List pings_sent;
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  for (const auto& token_and_data :
       WebUIInfoSingleton::GetInstance()->deep_scan_requests()) {
    pings_sent.Append(SerializeDeepScanDebugData(token_and_data.first,
                                                 token_and_data.second));
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

base::Value::Dict SafeBrowsingUIHandler::GetFormattedTailoredVerdictOverride() {
  base::Value::Dict override_dict;
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  const char kStatusKey[] = "status";
  const char kOverrideValueKey[] = "override_value";
  const TailoredVerdictOverrideData& override_data =
      WebUIInfoSingleton::GetInstance()->tailored_verdict_override();
  if (!override_data.override_value) {
    override_dict.Set(kStatusKey, base::Value("No override set."));
  } else {
    if (override_data.IsFromSource(this)) {
      override_dict.Set(kStatusKey, base::Value("Override set from this tab."));
    } else {
      override_dict.Set(kStatusKey,
                        base::Value("Override set from another tab."));
    }
    override_dict.Set(kOverrideValueKey,
                      proto_to_value::Serialize(*override_data.override_value));
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)
  return override_dict;
}

void SafeBrowsingUIHandler::SetTailoredVerdictOverride(
    const base::Value::List& args) {
  DCHECK_GE(args.size(), 2U);
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  ClientDownloadResponse::TailoredVerdict tv;
  const base::Value::Dict& input = args[1].GetDict();

  const std::string* tailored_verdict_type =
      input.FindString("tailored_verdict_type");
  CHECK(tailored_verdict_type);
  if (*tailored_verdict_type == "VERDICT_TYPE_UNSPECIFIED") {
    tv.set_tailored_verdict_type(
        ClientDownloadResponse::TailoredVerdict::VERDICT_TYPE_UNSPECIFIED);
  } else if (*tailored_verdict_type == "COOKIE_THEFT") {
    tv.set_tailored_verdict_type(
        ClientDownloadResponse::TailoredVerdict::COOKIE_THEFT);
  } else if (*tailored_verdict_type == "SUSPICIOUS_ARCHIVE") {
    tv.set_tailored_verdict_type(
        ClientDownloadResponse::TailoredVerdict::SUSPICIOUS_ARCHIVE);
  }

  WebUIInfoSingleton::GetInstance()->SetTailoredVerdictOverride(std::move(tv),
                                                                this);
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::GetTailoredVerdictOverride(
    const base::Value::List& args) {
  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ClearTailoredVerdictOverride(
    const base::Value::List& args) {
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  WebUIInfoSingleton::GetInstance()->ClearTailoredVerdictOverride();
#endif

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ResolveTailoredVerdictOverrideCallback(
    const std::string& callback_id) {
  AllowJavascript();
  ResolveJavascriptCallback(callback_id, GetFormattedTailoredVerdictOverride());
}

void SafeBrowsingUIHandler::NotifyTailoredVerdictOverrideJsListener() {
  AllowJavascript();
  FireWebUIListener("tailored-verdict-override-update",
                    GetFormattedTailoredVerdictOverride());
}

void SafeBrowsingUIHandler::NotifyDownloadUrlCheckedJsListener(
    const std::vector<GURL>& urls,
    DownloadCheckResult result) {
  AllowJavascript();
  FireWebUIListener("download-url-checked-update",
                    base::Value(SerializeDownloadUrlChecked(urls, result)));
}

void SafeBrowsingUIHandler::NotifyClientDownloadRequestJsListener(
    ClientDownloadRequest* client_download_request) {
  AllowJavascript();
  FireWebUIListener(
      "sent-client-download-requests-update",
      base::Value(SerializeClientDownloadRequest(*client_download_request)));
}

void SafeBrowsingUIHandler::NotifyClientDownloadResponseJsListener(
    ClientDownloadResponse* client_download_response) {
  AllowJavascript();
  FireWebUIListener(
      "received-client-download-responses-update",
      base::Value(SerializeClientDownloadResponse(*client_download_response)));
}

void SafeBrowsingUIHandler::NotifyClientPhishingRequestJsListener(
    const ClientPhishingRequestAndToken& client_phishing_request) {
  AllowJavascript();
  FireWebUIListener(
      "sent-client-phishing-requests-update",
      base::Value(SerializeClientPhishingRequest(client_phishing_request)));
}

void SafeBrowsingUIHandler::NotifyClientPhishingResponseJsListener(
    ClientPhishingResponse* client_phishing_response) {
  AllowJavascript();
  FireWebUIListener(
      "received-client-phishing-responses-update",
      base::Value(SerializeClientPhishingResponse(*client_phishing_response)));
}

void SafeBrowsingUIHandler::NotifyCSBRRJsListener(
    ClientSafeBrowsingReportRequest* csbrr) {
  AllowJavascript();
  FireWebUIListener("sent-csbrr-update", base::Value(SerializeCSBRR(*csbrr)));
}

void SafeBrowsingUIHandler::NotifyHitReportJsListener(HitReport* hit_report) {
  AllowJavascript();
  FireWebUIListener("sent-hit-report-list",
                    base::Value(SerializeHitReport(*hit_report)));
}

void SafeBrowsingUIHandler::NotifyPGEventJsListener(
    const sync_pb::UserEventSpecifics& event) {
  AllowJavascript();
  FireWebUIListener("sent-pg-event", SerializePGEvent(event));
}

void SafeBrowsingUIHandler::NotifySecurityEventJsListener(
    const sync_pb::GaiaPasswordReuse& event) {
  AllowJavascript();
  FireWebUIListener("sent-security-event", SerializeSecurityEvent(event));
}

void SafeBrowsingUIHandler::NotifyPGPingJsListener(
    int token,
    const LoginReputationClientRequestAndToken& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializePGPing(request));

  AllowJavascript();
  FireWebUIListener("pg-pings-update", request_list);
}

void SafeBrowsingUIHandler::NotifyPGResponseJsListener(
    int token,
    const LoginReputationClientResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(SerializePGResponse(response));

  AllowJavascript();
  FireWebUIListener("pg-responses-update", response_list);
}

void SafeBrowsingUIHandler::NotifyURTLookupPingJsListener(
    int token,
    const URTLookupRequest& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializeURTLookupPing(request));

  AllowJavascript();
  FireWebUIListener("urt-lookup-pings-update", request_list);
}

void SafeBrowsingUIHandler::NotifyURTLookupResponseJsListener(
    int token,
    const RTLookupResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(SerializeURTLookupResponse(response));

  AllowJavascript();
  FireWebUIListener("urt-lookup-responses-update", response_list);
}

void SafeBrowsingUIHandler::NotifyHPRTLookupPingJsListener(
    int token,
    const HPRTLookupRequest& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializeHPRTLookupPing(request));

  AllowJavascript();
  FireWebUIListener("hprt-lookup-pings-update", request_list);
}

void SafeBrowsingUIHandler::NotifyHPRTLookupResponseJsListener(
    int token,
    const V5::SearchHashesResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(SerializeHPRTLookupResponse(response));

  AllowJavascript();
  FireWebUIListener("hprt-lookup-responses-update", response_list);
}

void SafeBrowsingUIHandler::NotifyLogMessageJsListener(
    const base::Time& timestamp,
    const std::string& message) {
  AllowJavascript();
  FireWebUIListener("log-messages-update",
                    SerializeLogMessage(timestamp, message));
}

void SafeBrowsingUIHandler::NotifyReportingEventJsListener(
    const base::Value::Dict& event) {
  AllowJavascript();
  FireWebUIListener("reporting-events-update", SerializeReportingEvent(event));
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
void SafeBrowsingUIHandler::NotifyDeepScanJsListener(
    const std::string& token,
    const DeepScanDebugData& deep_scan_data) {
  AllowJavascript();
  FireWebUIListener("deep-scan-request-update",
                    SerializeDeepScanDebugData(token, deep_scan_data));
}
#endif

void SafeBrowsingUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExperiments",
      base::BindRepeating(&SafeBrowsingUIHandler::GetExperiments,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPolicies", base::BindRepeating(&SafeBrowsingUIHandler::GetPolicies,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrefs", base::BindRepeating(&SafeBrowsingUIHandler::GetPrefs,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCookie", base::BindRepeating(&SafeBrowsingUIHandler::GetCookie,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSavedPasswords",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSavedPasswords,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDatabaseManagerInfo",
      base::BindRepeating(&SafeBrowsingUIHandler::GetDatabaseManagerInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDownloadUrlsChecked",
      base::BindRepeating(&SafeBrowsingUIHandler::GetDownloadUrlsChecked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentClientDownloadRequests",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentClientDownloadRequests,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReceivedClientDownloadResponses",
      base::BindRepeating(
          &SafeBrowsingUIHandler::GetReceivedClientDownloadResponses,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentClientPhishingRequests",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentClientPhishingRequests,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReceivedClientPhishingResponses",
      base::BindRepeating(
          &SafeBrowsingUIHandler::GetReceivedClientPhishingResponses,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentCSBRRs",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentCSBRRs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentHitReports",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentHitReports,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGEvents", base::BindRepeating(&SafeBrowsingUIHandler::GetPGEvents,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSecurityEvents",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSecurityEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGPings", base::BindRepeating(&SafeBrowsingUIHandler::GetPGPings,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetPGResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getURTLookupPings",
      base::BindRepeating(&SafeBrowsingUIHandler::GetURTLookupPings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getURTLookupResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetURTLookupResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHPRTLookupPings",
      base::BindRepeating(&SafeBrowsingUIHandler::GetHPRTLookupPings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHPRTLookupResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetHPRTLookupResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&SafeBrowsingUIHandler::GetLogMessages,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReferrerChain",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReferrerChain,
                          base::Unretained(this)));
#if BUILDFLAG(IS_ANDROID)
  web_ui()->RegisterMessageCallback(
      "getReferringAppInfo",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReferringAppInfo,
                          base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "getReportingEvents",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReportingEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDeepScans", base::BindRepeating(&SafeBrowsingUIHandler::GetDeepScans,
                                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::GetTailoredVerdictOverride,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::SetTailoredVerdictOverride,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::ClearTailoredVerdictOverride,
                          base::Unretained(this)));
}

void SafeBrowsingUIHandler::SetWebUIForTesting(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

CrSBLogMessage::CrSBLogMessage() = default;

CrSBLogMessage::~CrSBLogMessage() {
  WebUIInfoSingleton::GetInstance()->LogMessage(stream_.str());
  DLOG(WARNING) << stream_.str();
}

}  // namespace safe_browsing
