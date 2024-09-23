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
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/enterprise/common/proto/connectors.pb.h"
#endif
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
using TriggeredRule =
    enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule;
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
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyDownloadUrlCheckedJsListener(urls, result);
  }
  download_urls_checked_.emplace_back(urls, result);
}

void WebUIInfoSingleton::AddToClientDownloadRequestsSent(
    std::unique_ptr<ClientDownloadRequest> client_download_request) {
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientDownloadRequestJsListener(
        client_download_request.get());
  }
  client_download_requests_sent_.push_back(std::move(client_download_request));
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
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientDownloadResponseJsListener(
        client_download_response.get());
  }
  client_download_responses_received_.push_back(
      std::move(client_download_response));
}

void WebUIInfoSingleton::ClearClientDownloadResponsesReceived() {
  std::vector<std::unique_ptr<ClientDownloadResponse>>().swap(
      client_download_responses_received_);
}

void WebUIInfoSingleton::AddToClientPhishingRequestsSent(
    std::unique_ptr<ClientPhishingRequest> client_phishing_request,
    std::string token) {
  if (!HasListener())
    return;
  ClientPhishingRequest request_copy = *client_phishing_request;
  ClientPhishingRequestAndToken ping = {request_copy, token};
  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientPhishingRequestJsListener(ping);
  }
  client_phishing_requests_sent_.push_back(ping);
}

void WebUIInfoSingleton::ClearClientPhishingRequestsSent() {
  std::vector<ClientPhishingRequestAndToken>().swap(
      client_phishing_requests_sent_);
}

void WebUIInfoSingleton::AddToClientPhishingResponsesReceived(
    std::unique_ptr<ClientPhishingResponse> client_phishing_response) {
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientPhishingResponseJsListener(
        client_phishing_response.get());
  }
  client_phishing_responses_received_.push_back(
      std::move(client_phishing_response));
}

void WebUIInfoSingleton::ClearClientPhishingResponsesReceived() {
  std::vector<std::unique_ptr<ClientPhishingResponse>>().swap(
      client_phishing_responses_received_);
}

void WebUIInfoSingleton::AddToCSBRRsSent(
    std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr) {
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyCSBRRJsListener(csbrr.get());
  }
  csbrrs_sent_.push_back(std::move(csbrr));
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
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyHitReportJsListener(hit_report.get());
  }
  hit_reports_sent_.push_back(std::move(hit_report));
}

void WebUIInfoSingleton::ClearHitReportsSent() {
  std::vector<std::unique_ptr<HitReport>>().swap(hit_reports_sent_);
}

void WebUIInfoSingleton::AddToPGEvents(
    const sync_pb::UserEventSpecifics& event) {
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyPGEventJsListener(event);
  }

  pg_event_log_.push_back(event);
}

void WebUIInfoSingleton::ClearPGEvents() {
  std::vector<sync_pb::UserEventSpecifics>().swap(pg_event_log_);
}

void WebUIInfoSingleton::AddToSecurityEvents(
    const sync_pb::GaiaPasswordReuse& event) {
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifySecurityEventJsListener(event);
  }

  security_event_log_.push_back(event);
}

void WebUIInfoSingleton::ClearSecurityEvents() {
  std::vector<sync_pb::GaiaPasswordReuse>().swap(security_event_log_);
}

int WebUIInfoSingleton::AddToPGPings(
    const LoginReputationClientRequest& request,
    const std::string oauth_token) {
  if (!HasListener())
    return -1;

  LoginReputationClientRequestAndToken ping = {request, oauth_token};

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyPGPingJsListener(pg_pings_.size(), ping);
  }

  pg_pings_.push_back(ping);

  return pg_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToPGResponses(
    int token,
    const LoginReputationClientResponse& response) {
  if (!HasListener())
    return;

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

int WebUIInfoSingleton::AddToURTLookupPings(const RTLookupRequest request,
                                            const std::string oauth_token) {
  if (!HasListener())
    return -1;

  URTLookupRequest ping = {request, oauth_token};

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyURTLookupPingJsListener(urt_lookup_pings_.size(),
                                                  ping);
  }

  urt_lookup_pings_.push_back(ping);

  return urt_lookup_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToURTLookupResponses(
    int token,
    const RTLookupResponse response) {
  if (!HasListener())
    return;

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
  HPRTLookupRequest request = {.inner_request = *inner_request,
                               .relay_url_spec = relay_url_spec,
                               .ohttp_key = ohttp_key};
  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyHPRTLookupPingJsListener(hprt_lookup_pings_.size(),
                                                   request);
  }
  hprt_lookup_pings_.push_back(request);
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
  if (!HasListener())
    return;

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
  if (!HasListener())
    return;

  for (safe_browsing::SafeBrowsingUIHandler* webui_listener :
       webui_instances_) {
    webui_listener->NotifyReportingEventJsListener(event);
  }

  reporting_events_.emplace_back(event.Clone());
}

void WebUIInfoSingleton::ClearReportingEvents() {
  std::vector<base::Value::Dict>().swap(reporting_events_);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void WebUIInfoSingleton::AddToDeepScanRequests(
    bool per_profile_request,
    const std::string& access_token,
    const std::string& upload_info,
    const std::string& upload_url,
    const enterprise_connectors::ContentAnalysisRequest& request) {
  if (!HasListener())
    return;

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
  if (!HasListener())
    return;

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
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

void WebUIInfoSingleton::RegisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  webui_instances_.push_back(webui);
}

void WebUIInfoSingleton::UnregisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  std::erase(webui_instances_, webui);

#if BUILDFLAG(FULL_SAFE_BROWSING)
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
ReferringAppInfo WebUIInfoSingleton::GetReferringAppInfo(
    content::WebContents* web_contents) {
  return sb_service_ ? sb_service_->GetReferringAppInfo(web_contents)
                     : ReferringAppInfo{};
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

#if BUILDFLAG(FULL_SAFE_BROWSING)
    ClearDeepScans();
    ClearTailoredVerdictOverride();
#endif
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
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
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

std::string UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds) {
  base::Time time =
      base::Time::UnixEpoch() + base::Milliseconds(time_in_milliseconds);
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

void AddStoreInfo(const DatabaseManagerInfo::DatabaseInfo::StoreInfo store_info,
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

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo database_info,
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

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo update_info,
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
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo
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
    full_hash_info_dict.Set("Full hash (base64)", full_hash);
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

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache full_hash_cache,
                        base::Value::List& full_hash_cache_list) {
  base::Value::Dict full_hash_cache_parsed;

  if (full_hash_cache.has_hash_prefix()) {
    std::string hash_prefix;
    base::Base64UrlEncode(full_hash_cache.hash_prefix(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &hash_prefix);
    full_hash_cache_parsed.Set("Hash prefix (base64)", hash_prefix);
  }
  if (full_hash_cache.cached_hash_prefix_info().has_negative_expiry()) {
    full_hash_cache_parsed.Set(
        "Negative expiry",
        UserReadableTimeFromMillisSinceEpoch(
            full_hash_cache.cached_hash_prefix_info().negative_expiry()));
  }

  full_hash_cache_list.Append(std::move(full_hash_cache_parsed));

  for (auto full_hash_info_it :
       full_hash_cache.cached_hash_prefix_info().full_hash_info()) {
    base::Value::Dict full_hash_info_dict;
    ParseFullHashInfo(full_hash_info_it, full_hash_info_dict);
    full_hash_cache_list.Append(std::move(full_hash_info_dict));
  }
}

void ParseFullHashCacheInfo(const FullHashCacheInfo full_hash_cache_info_proto,
                            base::Value::List& full_hash_cache_info) {
  if (full_hash_cache_info_proto.has_number_of_hits()) {
    base::Value::Dict number_of_hits;
    number_of_hits.Set("Number of cache hits",
                       full_hash_cache_info_proto.number_of_hits());
    full_hash_cache_info.Append(std::move(number_of_hits));
  }

  // Record FullHashCache list.
  for (auto full_hash_cache_it : full_hash_cache_info_proto.full_hash_cache()) {
    base::Value::List full_hash_cache_list;
    ParseFullHashCache(full_hash_cache_it, full_hash_cache_list);
    full_hash_cache_info.Append(std::move(full_hash_cache_list));
  }
}

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo full_hash_cache_info_proto) {
  std::string full_hash_cache_parsed;

  base::Value::List full_hash_cache;
  ParseFullHashCacheInfo(full_hash_cache_info_proto, full_hash_cache);

  JSONStringValueSerializer serializer(&full_hash_cache_parsed);
  serializer.set_pretty_print(true);
  serializer.Serialize(full_hash_cache);

  return full_hash_cache_parsed;
}

#endif

base::Value::Dict SerializeImageFeatureEmbedding(
    ImageFeatureEmbedding image_feature_embedding) {
  base::Value::Dict dict;
  base::Value::List embedding_values;
  for (const auto& value : image_feature_embedding.embedding_value()) {
    embedding_values.Append(value);
  }
  dict.Set("embedding_model_version",
           image_feature_embedding.embedding_model_version());
  dict.Set("embedding_value", std::move(embedding_values));
  return dict;
}

base::Value::Dict SerializeChromeUserPopulation(
    const ChromeUserPopulation& population) {
  base::Value::Dict population_dict;

  population_dict.Set(
      "user_population",
      ChromeUserPopulation_UserPopulation_Name(population.user_population()));

  if (population.has_is_history_sync_enabled()) {
    population_dict.Set("is_history_sync_enabled",
                        population.is_history_sync_enabled());
  }

  if (!population.finch_active_groups().empty()) {
    base::Value::List finch_list;
    for (const std::string& finch_group : population.finch_active_groups()) {
      finch_list.Append(finch_group);
    }
    population_dict.Set("finch_active_groups", std::move(finch_list));
  }

  if (population.has_profile_management_status()) {
    population_dict.Set("profile_management_status",
                        ChromeUserPopulation_ProfileManagementStatus_Name(
                            population.profile_management_status()));
  }
  if (population.has_is_under_advanced_protection()) {
    population_dict.Set("is_under_advanced_protection",
                        population.is_under_advanced_protection());
  }

  if (population.has_is_incognito()) {
    population_dict.Set("is_incognito", population.is_incognito());
  }

  if (population.has_is_mbb_enabled()) {
    population_dict.Set("is_mbb_enabled", population.is_mbb_enabled());
  }

  if (population.has_user_agent()) {
    population_dict.Set("user_agent", population.user_agent());
  }

  if (population.has_number_of_profiles()) {
    population_dict.Set("number_of_profiles", population.number_of_profiles());
  }

  if (population.has_number_of_loaded_profiles()) {
    population_dict.Set("number_of_loaded_profiles",
                        population.number_of_loaded_profiles());
  }

  if (population.has_number_of_open_profiles()) {
    population_dict.Set("number_of_open_profiles",
                        population.number_of_open_profiles());
  }

  base::Value::List page_load_tokens;
  for (const ChromeUserPopulation::PageLoadToken& token :
       population.page_load_tokens()) {
    base::Value::Dict token_dict;
    token_dict.Set("token_source",
                   ChromeUserPopulation_PageLoadToken_TokenSource_Name(
                       token.token_source()));
    token_dict.Set("token_time_msec",
                   static_cast<double>(token.token_time_msec()));

    std::string token_base64 = base::Base64Encode(token.token_value());
    token_dict.Set("token_value", token_base64);

    page_load_tokens.Append(std::move(token_dict));
  }
  population_dict.Set("page_load_tokens", std::move(page_load_tokens));

  if (population.has_is_signed_in()) {
    population_dict.Set("is_signed_in", population.is_signed_in());
  }

  return population_dict;
}

base::Value::Dict SerializeReferrer(const ReferrerChainEntry& referrer) {
  base::Value::Dict referrer_dict;
  referrer_dict.Set("url", referrer.url());
  referrer_dict.Set("main_frame_url", referrer.main_frame_url());
  referrer_dict.Set("type", ReferrerChainEntry_URLType_Name(referrer.type()));

  base::Value::List ip_addresses;
  for (const std::string& ip_address : referrer.ip_addresses()) {
    ip_addresses.Append(ip_address);
  }
  referrer_dict.Set("ip_addresses", std::move(ip_addresses));

  referrer_dict.Set("referrer_url", referrer.referrer_url());

  referrer_dict.Set("referrer_main_frame_url",
                    referrer.referrer_main_frame_url());

  if (referrer.has_is_retargeting()) {
    referrer_dict.Set("is_retargeting", referrer.is_retargeting());
  }

  referrer_dict.Set("navigation_time_msec", referrer.navigation_time_msec());

  base::Value::List server_redirects;
  for (const ReferrerChainEntry::ServerRedirect& server_redirect :
       referrer.server_redirect_chain()) {
    server_redirects.Append(server_redirect.url());
  }
  referrer_dict.Set("server_redirect_chain", std::move(server_redirects));

  referrer_dict.Set("navigation_initiation",
                    ReferrerChainEntry_NavigationInitiation_Name(
                        referrer.navigation_initiation()));

  if (referrer.has_maybe_launched_by_external_application()) {
    referrer_dict.Set("maybe_launched_by_external_application",
                      referrer.maybe_launched_by_external_application());
  }

  if (referrer.has_is_subframe_url_removed()) {
    referrer_dict.Set("is_subframe_url_removed",
                      referrer.is_subframe_url_removed());
  }

  if (referrer.has_is_subframe_referrer_url_removed()) {
    referrer_dict.Set("is_subframe_referrer_url_removed",
                      referrer.is_subframe_referrer_url_removed());
  }

  if (referrer.has_is_url_removed_by_policy()) {
    referrer_dict.Set("is_url_removed_by_policy",
                      referrer.is_url_removed_by_policy());
  }

  return referrer_dict;
}

std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr) {
  base::Value::Dict dict;
  if (cdr.has_url())
    dict.Set("url", cdr.url());
  if (cdr.digests().has_sha256()) {
    const std::string& sha256 = cdr.digests().sha256();
    dict.Set("digests.sha256", base::HexEncode(sha256));
  }
  if (cdr.has_download_type())
    dict.Set("download_type", cdr.download_type());
  if (cdr.has_length())
    dict.Set("length", static_cast<int>(cdr.length()));
  if (cdr.has_file_basename())
    dict.Set("file_basename", cdr.file_basename());

  if (!cdr.archived_binary().empty()) {
    base::Value::List archived_binaries;
    for (const auto& archived_binary : cdr.archived_binary()) {
      base::Value::Dict dict_archived_binary;
      if (archived_binary.has_file_path()) {
        dict_archived_binary.Set("file_path", archived_binary.file_path());
      }
      if (archived_binary.has_download_type()) {
        dict_archived_binary.Set("download_type",
                                 archived_binary.download_type());
      }
      if (archived_binary.has_length())
        dict_archived_binary.Set("length",
                                 static_cast<int>(archived_binary.length()));
      if (archived_binary.is_encrypted())
        dict_archived_binary.Set("is_encrypted", true);
      if (archived_binary.digests().has_sha256()) {
        const std::string& sha256 = archived_binary.digests().sha256();
        dict_archived_binary.Set("digests.sha256", base::HexEncode(sha256));
      }
      archived_binaries.Append(std::move(dict_archived_binary));
    }
    dict.Set("archived_binary", std::move(archived_binaries));
  }

  dict.Set("population", SerializeChromeUserPopulation(cdr.population()));

  base::Value::List referrer_chain;
  for (const auto& referrer_chain_entry : cdr.referrer_chain()) {
    referrer_chain.Append(SerializeReferrer(referrer_chain_entry));
  }
  dict.Set("referrer_chain", std::move(referrer_chain));

  if (cdr.has_request_ap_verdicts())
    dict.Set("request_ap_verdicts", cdr.request_ap_verdicts());

  if (!cdr.access_token().empty())
    dict.Set("access_token", cdr.access_token());

  if (cdr.has_archive_summary()) {
    base::Value::Dict dict_archive_summary;
    auto archive_summary = cdr.archive_summary();
    dict_archive_summary.Set("parser_status", archive_summary.parser_status());
    dict_archive_summary.Set("file_count", archive_summary.file_count());
    dict_archive_summary.Set("directory_count",
                             archive_summary.directory_count());
    dict_archive_summary.Set("is_encrypted", archive_summary.is_encrypted());
    dict.Set("archive_summary", std::move(dict_archive_summary));
  }

  if (cdr.has_tailored_info()) {
    base::Value::Dict dict_tailored_info;
    auto tailored_info = cdr.tailored_info();
    dict_tailored_info.Set("version", tailored_info.version());
    dict.Set("tailored_info", std::move(dict_tailored_info));
  }

  if (cdr.has_previous_token()) {
    dict.Set("previous_token", cdr.previous_token());
  }

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(dict);
  return request_serialized;
}

base::Value::Dict SerializeTailoredVerdict(
    const ClientDownloadResponse::TailoredVerdict& tv) {
  base::Value::Dict dict_tailored_verdict;
  dict_tailored_verdict.Set(
      "tailored_verdict_type",
      ClientDownloadResponse_TailoredVerdict_TailoredVerdictType_Name(
          tv.tailored_verdict_type()));
  base::Value::List adjustments;
  for (const auto& adjustment : tv.adjustments()) {
    adjustments.Append(
        ClientDownloadResponse_TailoredVerdict_ExperimentalWarningAdjustment_Name(
            adjustment));
  }
  dict_tailored_verdict.Set("adjustments", std::move(adjustments));
  return dict_tailored_verdict;
}

std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr) {
  base::Value::Dict dict;

  if (cdr.has_verdict()) {
    dict.Set("verdict", ClientDownloadResponse_Verdict_Name(cdr.verdict()));
  }

  if (cdr.has_more_info()) {
    dict.SetByDottedPath("more_info.description",
                         cdr.more_info().description());
    dict.SetByDottedPath("more_info.url", cdr.more_info().url());
  }

  if (cdr.has_token()) {
    dict.Set("token", cdr.token());
  }

  if (cdr.has_upload()) {
    dict.Set("upload", cdr.upload());
  }

  if (cdr.has_request_deep_scan()) {
    dict.Set("request_deep_scan", cdr.request_deep_scan());
  }

  if (cdr.has_tailored_verdict()) {
    auto tailored_verdict = cdr.tailored_verdict();
    dict.Set("tailored_verdict", SerializeTailoredVerdict(tailored_verdict));
  }

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(dict);
  return request_serialized;
}

base::Value::Dict SerializeVisualFeatures(
    const VisualFeatures& visual_features) {
  base::Value::Dict image_dict;
  const VisualFeatures::BlurredImage& image = visual_features.image();
  image_dict.Set("width", image.width());
  image_dict.Set("height", image.height());
  image_dict.Set("data", base::Base64Encode(
                             base::as_bytes(base::make_span(image.data()))));

  base::Value::Dict visual_dict;
  visual_dict.Set("blurred_image", std::move(image_dict));
  return visual_dict;
}

std::string SerializeClientPhishingRequest(
    const ClientPhishingRequestAndToken& cprat) {
  const ClientPhishingRequest& cpr = cprat.request;
  base::Value::Dict dict;
  if (cpr.has_url())
    dict.Set("url", cpr.url());
  if (cpr.has_client_score())
    dict.Set("client_score", cpr.client_score());
  if (cpr.has_is_phishing())
    dict.Set("is_phishing", cpr.is_phishing());
  if (cpr.has_model_version())
    dict.Set("model_version", cpr.model_version());
  if (cpr.has_dom_model_version())
    dict.Set("dom_model_version", cpr.dom_model_version());
  if (cpr.has_client_side_detection_type()) {
    dict.Set("client_side_detection_type",
             ClientSideDetectionType_Name(cpr.client_side_detection_type()));
  }
  if (cpr.has_report_type()) {
    dict.Set("report_type",
             ClientPhishingRequest_ReportType_Name(cpr.report_type()));
  }

  if (cpr.has_image_feature_embedding()) {
    dict.Set("image_feature_embedding",
             SerializeImageFeatureEmbedding(cpr.image_feature_embedding()));
  }

  base::Value::List features;
  for (const auto& feature : cpr.feature_map()) {
    base::Value::Dict dict_features;
    dict_features.Set("name", feature.name());
    dict_features.Set("value", feature.value());
    features.Append(std::move(dict_features));
  }
  dict.Set("feature_map", std::move(features));

  base::Value::List non_model_features;
  for (const auto& feature : cpr.non_model_feature_map()) {
    base::Value::Dict dict_features;
    dict_features.Set("name", feature.name());
    dict_features.Set("value", feature.value());
    non_model_features.Append(std::move(dict_features));
  }
  dict.Set("non_model_feature_map", std::move(non_model_features));

  base::Value::List shingle_hashes;
  for (const auto& hash : cpr.shingle_hashes()) {
    shingle_hashes.Append(static_cast<int>(hash));
  }
  dict.Set("shingle_hashes", std::move(shingle_hashes));

  dict.Set("population", SerializeChromeUserPopulation(cpr.population()));
  dict.Set("is_dom_match", cpr.is_dom_match());
  dict.Set("scoped_oauth_token", cprat.token);

  if (cpr.has_tflite_model_version())
    dict.Set("tflite_model_version", cpr.tflite_model_version());
  dict.Set("is_tflite_match", cpr.is_tflite_match());

  base::Value::List tflite_scores;
  for (const auto& score : cpr.tflite_model_scores()) {
    base::Value::Dict score_value;
    score_value.Set("label", score.label());
    score_value.Set("lvalue", score.value());
    tflite_scores.Append(std::move(score_value));
  }
  dict.Set("tflite_model_scores", std::move(tflite_scores));

  if (cpr.has_visual_features()) {
    dict.Set("visual_features", SerializeVisualFeatures(cpr.visual_features()));
  }

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(dict);
  return request_serialized;
}

std::string SerializeClientPhishingResponse(const ClientPhishingResponse& cpr) {
  base::Value::Dict dict;
  dict.Set("phishy", cpr.phishy());

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(dict);
  return request_serialized;
}

base::Value::Dict SerializeHTTPHeader(
    const ClientSafeBrowsingReportRequest::HTTPHeader& header) {
  base::Value::Dict header_dict;
  header_dict.Set("name", header.name());
  header_dict.Set("value", header.value());
  return header_dict;
}

base::Value::Dict SerializeResource(
    const ClientSafeBrowsingReportRequest::Resource& resource) {
  base::Value::Dict resource_dict;
  resource_dict.Set("id", resource.id());
  resource_dict.Set("url", resource.url());
  // HTTPRequest
  if (resource.has_request()) {
    base::Value::Dict request;
    if (resource.request().has_firstline()) {
      base::Value::Dict firstline;
      firstline.Set("verb", resource.request().firstline().verb());
      firstline.Set("uri", resource.request().firstline().uri());
      firstline.Set("version", resource.request().firstline().version());
      request.Set("firstline", std::move(firstline));
    }
    base::Value::List headers;
    for (const ClientSafeBrowsingReportRequest::HTTPHeader& header :
         resource.request().headers()) {
      headers.Append(SerializeHTTPHeader(header));
    }
    request.Set("headers", std::move(headers));
    resource_dict.Set("request", std::move(request));
  }
  // HTTPResponse
  if (resource.has_response()) {
    base::Value::Dict response;
    if (resource.response().has_firstline()) {
      base::Value::Dict firstline;
      firstline.Set("code", resource.response().firstline().code());
      firstline.Set("message", resource.response().firstline().message());
      firstline.Set("version", resource.response().firstline().version());
      response.Set("firstline", std::move(firstline));
    }
    base::Value::List headers;
    for (const ClientSafeBrowsingReportRequest::HTTPHeader& header :
         resource.response().headers()) {
      headers.Append(SerializeHTTPHeader(header));
    }
    response.Set("headers", std::move(headers));
    response.Set("body", resource.response().body());
    response.Set("remote_ip", resource.response().remote_ip());
    resource_dict.Set("response", std::move(response));
  }
  resource_dict.Set("parent_id", resource.parent_id());
  base::Value::List child_id_list;
  for (const int& child_id : resource.child_ids()) {
    child_id_list.Append(child_id);
  }
  resource_dict.Set("child_ids", std::move(child_id_list));
  resource_dict.Set("tag_name", resource.tag_name());
  return resource_dict;
}

base::Value::Dict SerializeHTMLElement(const HTMLElement& element) {
  base::Value::Dict element_dict;
  element_dict.Set("id", element.id());
  element_dict.Set("tag", element.tag());
  base::Value::List child_id_lists;
  for (const int& child_id : element.child_ids()) {
    child_id_lists.Append(child_id);
  }
  element_dict.Set("child_ids", std::move(child_id_lists));
  element_dict.Set("resource_id", element.resource_id());
  base::Value::List attribute_list;
  for (const HTMLElement::Attribute& attribute : element.attribute()) {
    base::Value::Dict attribute_dict;
    attribute_dict.Set("name", attribute.name());
    attribute_dict.Set("value", attribute.value());
    attribute_list.Append(std::move(attribute_dict));
  }
  element_dict.Set("attribute", std::move(attribute_list));
  element_dict.Set("inner_html", element.inner_html());
  return element_dict;
}

base::Value::Dict SerializeSafeBrowsingClientProperties(
    const ClientSafeBrowsingReportRequest::SafeBrowsingClientProperties&
        client_properties) {
  base::Value::Dict client_properties_dict;
  client_properties_dict.Set("client_version",
                             client_properties.client_version());
  client_properties_dict.Set(
      "google_play_services_version",
      static_cast<int>(client_properties.google_play_services_version()));
  client_properties_dict.Set("is_instant_apps",
                             client_properties.is_instant_apps());
  client_properties_dict.Set(
      "url_api_type",
      ClientSafeBrowsingReportRequest_SafeBrowsingUrlApiType_Name(
          client_properties.url_api_type()));
  client_properties_dict.Set("is_async_check",
                             client_properties.is_async_check());
  if (client_properties.has_app_verification_enabled()) {
    client_properties_dict.Set("app_verification_enabled",
                               client_properties.app_verification_enabled());
  }
  return client_properties_dict;
}

base::Value::Dict SerializeDownloadWarningAction(
    const ClientSafeBrowsingReportRequest::DownloadWarningAction&
        download_warning_action) {
  base::Value::Dict action_dict;
  action_dict.Set(
      "surface",
      ClientSafeBrowsingReportRequest_DownloadWarningAction_Surface_Name(
          download_warning_action.surface()));
  action_dict.Set(
      "action",
      ClientSafeBrowsingReportRequest_DownloadWarningAction_Action_Name(
          download_warning_action.action()));
  action_dict.Set("is_terminal_action",
                  download_warning_action.is_terminal_action());
  action_dict.Set("interval_msec",
                  static_cast<double>(download_warning_action.interval_msec()));
  return action_dict;
}

std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report) {
  base::Value::Dict report_request;
  if (report.has_type()) {
    report_request.Set(
        "type", ClientSafeBrowsingReportRequest_ReportType_Name(report.type()));
  }
  if (report.has_page_url()) {
    report_request.Set("page_url", report.page_url());
  }
  if (report.has_referrer_url()) {
    report_request.Set("referrer_url", report.referrer_url());
  }
  if (report.has_client_country()) {
    report_request.Set("client_country", report.client_country());
  }
  if (report.has_repeat_visit()) {
    report_request.Set("repeat_visit", report.repeat_visit());
  }
  if (report.has_did_proceed()) {
    report_request.Set("did_proceed", report.did_proceed());
  }
  if (report.has_download_verdict()) {
    report_request.Set("download_verdict", ClientDownloadResponse_Verdict_Name(
                                               report.download_verdict()));
  }
  if (report.has_url()) {
    report_request.Set("url", report.url());
  }
  if (report.has_token()) {
    report_request.Set("token", report.token());
  }
  if (report.has_show_download_in_folder()) {
    report_request.Set("show_download_in_folder",
                       report.show_download_in_folder());
  }
  if (report.has_population()) {
    report_request.Set("population",
                       SerializeChromeUserPopulation(report.population()));
  }
  base::Value::List resource_list;
  for (const ClientSafeBrowsingReportRequest::Resource& resource :
       report.resources()) {
    resource_list.Append(SerializeResource(resource));
  }
  report_request.Set("resources", std::move(resource_list));
  base::Value::List dom_list;
  for (const HTMLElement& element : report.dom()) {
    dom_list.Append(SerializeHTMLElement(element));
  }
  report_request.Set("dom", std::move(dom_list));
  if (report.has_complete()) {
    report_request.Set("complete", report.complete());
  }
  if (report.has_client_properties()) {
    report_request.Set(
        "client_properties",
        SerializeSafeBrowsingClientProperties(report.client_properties()));
  }
  base::Value::List download_warning_action_list;
  for (const auto& download_warning_action :
       report.download_warning_actions()) {
    download_warning_action_list.Append(
        SerializeDownloadWarningAction(download_warning_action));
  }
  report_request.Set("download_warning_actions",
                     std::move(download_warning_action_list));
  if (report.has_url_request_destination()) {
    report_request.Set(
        "url_request_destination",
        ClientSafeBrowsingReportRequest_UrlRequestDestination_Name(
            report.url_request_destination()));
  }
  if (report.has_warning_shown_timestamp_msec()) {
    report_request.Set(
        "warning_shown_timestamp_msec",
        static_cast<double>(report.warning_shown_timestamp_msec()));
  }
  std::string serialized;
  if (report.SerializeToString(&serialized)) {
    std::string base64_encoded = base::Base64Encode(serialized);
    report_request.Set("csbrr(base64)", base64_encoded);
  }
  std::string report_request_serialized;
  JSONStringValueSerializer serializer(&report_request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(report_request);
  return report_request_serialized;
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
  hit_report_dict.Set("threat_type", threat_type);
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
  hit_report_dict.Set("threat_source", threat_source);
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
  hit_report_dict.Set("extended_reporting_level", extended_reporting_level);
  hit_report_dict.Set("is_enhanced_protection",
                      hit_report.is_enhanced_protection);
  hit_report_dict.Set("is_metrics_reporting_active",
                      hit_report.is_metrics_reporting_active);
  hit_report_dict.Set("post_data", hit_report.post_data);
  std::string hit_report_serialized;
  JSONStringValueSerializer serializer(&hit_report_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(hit_report_dict);
  return hit_report_serialized;
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

  std::string event_serialized;
  JSONStringValueSerializer serializer(&event_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(event_dict);
  result.Set("message", event_serialized);
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

  std::string event_serialized;
  JSONStringValueSerializer serializer(&event_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(event_dict);
  result.Set("message", event_serialized);
  return result;
}

base::Value::Dict SerializeFrame(
    const LoginReputationClientRequest::Frame& frame) {
  base::Value::Dict frame_dict;
  frame_dict.Set("frame_index", frame.frame_index());
  frame_dict.Set("parent_frame_index", frame.parent_frame_index());
  frame_dict.Set("url", frame.url());
  frame_dict.Set("has_password_field", frame.has_password_field());

  base::Value::List referrer_list;
  for (const ReferrerChainEntry& referrer : frame.referrer_chain()) {
    referrer_list.Append(SerializeReferrer(referrer));
  }
  frame_dict.Set("referrer_chain", std::move(referrer_list));

  frame_dict.SetByDottedPath(
      "referrer_chain_options.recent_navigations_to_collect",
      frame.referrer_chain_options().recent_navigations_to_collect());

  base::Value::List form_list;
  for (const LoginReputationClientRequest::Frame::Form& form : frame.forms()) {
    base::Value::Dict form_dict;
    form_dict.Set("action_url", form.action_url());
    form_dict.Set("has_password_field", form.has_password_field());
    form_list.Append(std::move(form_dict));
  }
  frame_dict.Set("forms", std::move(form_list));

  return frame_dict;
}

base::Value::Dict SerializePasswordReuseEvent(
    const LoginReputationClientRequest::PasswordReuseEvent& event) {
  base::Value::Dict event_dict;

  base::Value::List domains_list;
  for (const std::string& domain : event.domains_matching_password()) {
    domains_list.Append(domain);
  }
  event_dict.Set("domains_matching_password", std::move(domains_list));

  event_dict.Set("frame_id", event.frame_id());

  event_dict.Set(
      "sync_account_type",
      LoginReputationClientRequest_PasswordReuseEvent_SyncAccountType_Name(
          event.sync_account_type()));

  event_dict.Set(
      "reused_password_type",
      LoginReputationClientRequest_PasswordReuseEvent_ReusedPasswordType_Name(
          event.reused_password_type()));

  event_dict.Set(
      "reused_password_account_type",
      LoginReputationClientRequest_PasswordReuseEvent_ReusedPasswordAccountType_AccountType_Name(
          event.reused_password_account_type().account_type()));
  event_dict.Set("is_account_syncing",
                 event.reused_password_account_type().is_account_syncing());

  return event_dict;
}

base::Value::Dict SerializeRTThreatInfo(
    const RTLookupResponse::ThreatInfo& threat_info) {
  base::Value::Dict threat_info_dict;

  threat_info_dict.Set(
      "threat_type",
      RTLookupResponse_ThreatInfo_ThreatType_Name(threat_info.threat_type()));

  threat_info_dict.Set("cache_duration_sec",
                       static_cast<double>(threat_info.cache_duration_sec()));

  threat_info_dict.Set(
      "verdict_type",
      RTLookupResponse_ThreatInfo_VerdictType_Name(threat_info.verdict_type()));

  threat_info_dict.Set(
      "cache_expression_match_type",
      RTLookupResponse_ThreatInfo_CacheExpressionMatchType_Name(
          threat_info.cache_expression_match_type()));
  threat_info_dict.Set("cache_expression_using_match_type",
                       threat_info.cache_expression_using_match_type());

  if (threat_info.has_matched_url_navigation_rule()) {
    base::Value::Dict matched_rule;
    matched_rule.Set("rule_id",
                     threat_info.matched_url_navigation_rule().rule_id());
    matched_rule.Set("rule_name",
                     threat_info.matched_url_navigation_rule().rule_name());
    matched_rule.Set(
        "matched_url_category",
        threat_info.matched_url_navigation_rule().matched_url_category());

    if (threat_info.matched_url_navigation_rule().has_custom_message()) {
      base::Value::List message_segments;
      for (const auto& segment : threat_info.matched_url_navigation_rule()
                                     .custom_message()
                                     .message_segments()) {
        base::Value::Dict segment_value;
        if (segment.has_text()) {
          segment_value.Set("text", segment.text());
        }
        if (segment.has_link()) {
          segment_value.Set("link", segment.link());
        }
        message_segments.Append(std::move(segment_value));
      }
      matched_rule.Set("message_segments", std::move(message_segments));
    }
    threat_info_dict.Set("matched_url_navigation_rule",
                         std::move(matched_rule));
  }

  return threat_info_dict;
}

base::Value::Dict SerializeDomFeatures(const DomFeatures& dom_features) {
  base::Value::Dict dom_features_dict;
  base::Value::List feature_map;
  for (const auto& feature : dom_features.feature_map()) {
    base::Value::Dict feature_dict;
    feature_dict.Set("name", feature.name());
    feature_dict.Set("value", feature.value());
    feature_map.Append(std::move(feature_dict));
  }
  dom_features_dict.Set("feature_map", std::move(feature_map));

  base::Value::List shingle_hashes;
  for (const auto& hash : dom_features.shingle_hashes()) {
    shingle_hashes.Append(static_cast<int>(hash));
  }
  dom_features_dict.Set("shingle_hashes", std::move(shingle_hashes));

  dom_features_dict.Set("model_version", dom_features.model_version());

  return dom_features_dict;
}

base::Value::Dict SerializeUrlDisplayExperiment(
    const LoginReputationClientRequest::UrlDisplayExperiment& experiment) {
  base::Value::Dict d;
  d.Set("delayed_warnings_enabled", experiment.delayed_warnings_enabled());
  d.Set("delayed_warnings_mouse_clicks_enabled",
        experiment.delayed_warnings_mouse_clicks_enabled());
  d.Set("reveal_on_hover", experiment.reveal_on_hover());
  d.Set("hide_on_interaction", experiment.hide_on_interaction());
  d.Set("elide_to_registrable_domain",
        experiment.elide_to_registrable_domain());
  return d;
}

#if BUILDFLAG(IS_ANDROID)
base::Value::Dict SerializeReferringAppInfo(const ReferringAppInfo& info) {
  base::Value::Dict dict;
  dict.Set(
      "referring_app_source",
      LoginReputationClientRequest_ReferringAppInfo_ReferringAppSource_Name(
          info.referring_app_source));
  dict.Set("referring_app_info", info.referring_app_name);
  dict.Set("target_url", info.target_url.spec());
  return dict;
}
#endif

base::Value::Dict SerializeReferringAppInfo(
    const LoginReputationClientRequest::ReferringAppInfo& info) {
  base::Value::Dict dict;
  dict.Set(
      "referring_app_source",
      LoginReputationClientRequest_ReferringAppInfo_ReferringAppSource_Name(
          info.referring_app_source()));
  dict.Set("referring_app_info", info.referring_app_name());
  return dict;
}

base::Value::Dict SerializeCsdDebuggingMetadata(
    const LoginReputationClientRequest::DebuggingMetadata& debugging_metadata) {
  base::Value::Dict dict;

  if (debugging_metadata.has_csd_model_version()) {
    dict.Set("csd_model_version", debugging_metadata.csd_model_version());
  }

  if (debugging_metadata.has_preclassification_check_result()) {
    dict.Set("preclassification_check_result",
             PreClassificationCheckResult_Name(
                 debugging_metadata.preclassification_check_result()));
  }

  if (debugging_metadata.has_phishing_detector_result()) {
    dict.Set("phishing_detector_result",
             PhishingDetectorResult_Name(
                 debugging_metadata.phishing_detector_result()));
  }

  if (debugging_metadata.has_local_model_detects_phishing()) {
    dict.Set("local_model_detects_phishing",
             debugging_metadata.local_model_detects_phishing());
  }

  if (debugging_metadata.has_forced_request()) {
    dict.Set("forced_request", debugging_metadata.forced_request());
  }

  if (debugging_metadata.has_network_result()) {
    dict.Set("network_result", debugging_metadata.network_result());
  }

  return dict;
}

std::string SerializePGPing(
    const LoginReputationClientRequestAndToken& request_and_token) {
  base::Value::Dict request_dict;

  const LoginReputationClientRequest& request = request_and_token.request;

  request_dict.Set("page_url", request.page_url());

  request_dict.Set(
      "trigger_type",
      LoginReputationClientRequest_TriggerType_Name(request.trigger_type()));

  base::Value::List frames_list;
  for (const LoginReputationClientRequest::Frame& frame : request.frames()) {
    frames_list.Append(SerializeFrame(frame));
  }
  request_dict.Set("frames", std::move(frames_list));

  request_dict.Set("password_reuse_event",
                   SerializePasswordReuseEvent(request.password_reuse_event()));
  request_dict.Set("stored_verdict_cnt", request.stored_verdict_cnt());
  request_dict.Set("population",
                   SerializeChromeUserPopulation(request.population()));
  request_dict.Set("clicked_through_interstitial",
                   request.clicked_through_interstitial());
  request_dict.Set("content_type", request.content_type());

  if (request.has_content_area_height()) {
    request_dict.Set("content_area_height", request.content_area_height());
  }
  if (request.has_content_area_width()) {
    request_dict.Set("content_area_width", request.content_area_width());
  }

  if (request.has_dom_features()) {
    request_dict.Set("dom_features",
                     SerializeDomFeatures(request.dom_features()));
  }

  if (request.has_url_display_experiment()) {
    request_dict.Set(
        "url_display_experiment",
        SerializeUrlDisplayExperiment(request.url_display_experiment()));
  }

  if (request.has_referring_app_info()) {
    request_dict.Set("referring_app_info",
                     SerializeReferringAppInfo(request.referring_app_info()));
  }

  if (request.has_visual_features()) {
    request_dict.Set("visual_features",
                     SerializeVisualFeatures(request.visual_features()));
  }

  if (request.has_csd_debugging_metadata()) {
    request_dict.Set(
        "csd_debugging_metadata",
        SerializeCsdDebuggingMetadata(request.csd_debugging_metadata()));
  }

  request_dict.Set("scoped_oauth_token", request_and_token.token);

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(request_dict);
  return request_serialized;
}

std::string SerializePGResponse(const LoginReputationClientResponse& response) {
  base::Value::Dict response_dict;

  response_dict.Set(
      "verdict_type",
      LoginReputationClientResponse_VerdictType_Name(response.verdict_type()));
  response_dict.Set("cache_duration_sec",
                    static_cast<int>(response.cache_duration_sec()));
  response_dict.Set("cache_expression", response.cache_expression());
  response_dict.Set("verdict_token", response.verdict_token());

  std::string response_serialized;
  JSONStringValueSerializer serializer(&response_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(response_dict);
  return response_serialized;
}

std::string SerializeURTLookupPing(const URTLookupRequest& ping) {
  base::Value::Dict request_dict;
  RTLookupRequest request = ping.request;

  request_dict.Set("url", request.url());
  request_dict.Set("population",
                   SerializeChromeUserPopulation(request.population()));
  request_dict.Set("scoped_oauth_token", ping.token);
  request_dict.Set("dm_token", request.dm_token());
  request_dict.Set("profile_dm_token", request.profile_dm_token());
  request_dict.Set("browser_dm_token", request.browser_dm_token());
  request_dict.Set("email", request.email());

  request_dict.Set("lookup_type",
                   RTLookupRequest_LookupType_Name(request.lookup_type()));

  request_dict.Set("version", request.version());

  request_dict.Set("os", RTLookupRequest_OSType_Name(request.os_type()));

  base::Value::List referrer_chain;
  for (const auto& referrer_chain_entry : request.referrer_chain()) {
    referrer_chain.Append(SerializeReferrer(referrer_chain_entry));
  }
  request_dict.Set("referrer_chain", std::move(referrer_chain));

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(request_dict);
  return request_serialized;
}

std::string SerializeURTLookupResponse(const RTLookupResponse& response) {
  base::Value::Dict response_dict;

  base::Value::List threat_info_list;
  for (const RTLookupResponse::ThreatInfo& threat_info :
       response.threat_info()) {
    threat_info_list.Append(SerializeRTThreatInfo(threat_info));
  }
  response_dict.Set("threat_infos", std::move(threat_info_list));

  response_dict.Set(
      "client_side_detection_type",
      ClientSideDetectionType_Name(response.client_side_detection_type()));

  base::Value::List url_categories_list;
  for (const std::string& url_category : response.url_categories()) {
    url_categories_list.Append(url_category);
  }
  response_dict.Set("url_categories", std::move(url_categories_list));

  std::string response_serialized;
  JSONStringValueSerializer serializer(&response_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(response_dict);
  return response_serialized;
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
    encoded_hash_prefixes.Append(encoded_hash_prefix);
  }
  inner_request_dict.Set("hash_prefixes (base64)",
                         std::move(encoded_hash_prefixes));

  request_dict.Set("inner_request", std::move(inner_request_dict));
  request_dict.Set("relay_url", ping.relay_url_spec);
  std::string encoded_ohttp_key;
  base::Base64UrlEncode(ping.ohttp_key,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_ohttp_key);
  request_dict.Set("ohttp_public_key (base64)", encoded_ohttp_key);

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(request_dict);
  return request_serialized;
}

std::string SerializeHPRTLookupResponse(
    const V5::SearchHashesResponse& response) {
  base::Value::Dict response_dict;

  // full_hashes
  base::Value::List full_hashes_list;
  for (const auto& full_hash : response.full_hashes()) {
    base::Value::Dict full_hash_dict;
    // full_hash
    std::string encoded_full_hash;
    base::Base64UrlEncode(full_hash.full_hash(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded_full_hash);
    full_hash_dict.Set("full_hash (base64)", encoded_full_hash);
    // full_hash_details
    base::Value::List full_hash_details_list;
    for (const auto& full_hash_detail : full_hash.full_hash_details()) {
      base::Value::Dict full_hash_detail_dict;
      // threat_type
      full_hash_detail_dict.Set(
          "threat_type", ThreatType_Name(full_hash_detail.threat_type()));
      // attributes
      base::Value::List attributes_list;
      for (auto i = 0; i < full_hash_detail.attributes_size(); ++i) {
        attributes_list.Append(
            ThreatAttribute_Name(full_hash_detail.attributes(i)));
      }
      full_hash_detail_dict.Set("attributes", std::move(attributes_list));

      full_hash_details_list.Append(std::move(full_hash_detail_dict));
    }
    full_hash_dict.Set("full_hash_details", std::move(full_hash_details_list));

    full_hashes_list.Append(std::move(full_hash_dict));
  }
  response_dict.Set("full_hashes", std::move(full_hashes_list));

  // cache_duration
  base::Value::Dict cache_duration_dict;
  cache_duration_dict.Set(
      "seconds", static_cast<double>(response.cache_duration().seconds()));
  cache_duration_dict.Set(
      "nanos", static_cast<double>(response.cache_duration().nanos()));
  response_dict.Set("cache_duration", std::move(cache_duration_dict));

  std::string response_serialized;
  JSONStringValueSerializer serializer(&response_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(response_dict);
  return response_serialized;
}

base::Value::Dict SerializeLogMessage(const base::Time& timestamp,
                                      const std::string& message) {
  base::Value::Dict result;
  result.Set("time", timestamp.InMillisecondsFSinceUnixEpoch());
  result.Set("message", message);
  return result;
}

base::Value::Dict SerializeReportingEvent(const base::Value::Dict& event) {
  base::Value::Dict result;

  std::string event_serialized;
  JSONStringValueSerializer serializer(&event_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(event);

  result.Set("message", event_serialized);

  return result;
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
std::string SerializeContentAnalysisRequest(
    bool per_profile_request,
    const std::string& access_token_truncated,
    const std::string& upload_info,
    const std::string& upload_url,
    const enterprise_connectors::ContentAnalysisRequest& request) {
  base::Value::Dict request_dict;

  request_dict.Set(per_profile_request ? "profile_token" : "device_token",
                   request.device_token());
  request_dict.Set("fcm_notification_token", request.fcm_notification_token());
  request_dict.Set("blocking", request.blocking());
  request_dict.Set("analysis_connector",
                   enterprise_connectors::AnalysisConnector_Name(
                       request.analysis_connector()));
  request_dict.Set("reason",
                   enterprise_connectors::ContentAnalysisRequest_Reason_Name(
                       request.reason()));

  if (request.has_request_data()) {
    base::Value::Dict request_data;
    request_data.Set("url", request.request_data().url());
    request_data.Set("filename", request.request_data().filename());
    request_data.Set("digest", request.request_data().digest());
    if (request.request_data().has_csd()) {
      std::string csd_base64 =
          base::Base64Encode(request.request_data().csd().SerializeAsString());
      request_data.Set("csd", csd_base64);
    }
    request_data.Set("content_type", request.request_data().content_type());
    request_data.Set("tab_url", request.request_data().tab_url());
    request_data.Set("source", request.request_data().source());
    request_data.Set("destination", request.request_data().destination());
    request_data.Set("email", request.request_data().email());

    request_dict.Set("request_data", std::move(request_data));
  }

  if (request.has_client_metadata()) {
    base::Value::Dict metadata;

    if (request.client_metadata().has_browser()) {
      const auto& browser = request.client_metadata().browser();
      base::Value::Dict browser_metadata;
      browser_metadata.Set("browser_id", browser.browser_id());
      browser_metadata.Set("user_agent", browser.user_agent());
      browser_metadata.Set("chrome_version", browser.chrome_version());
      browser_metadata.Set("machine_user", browser.machine_user());
      metadata.Set("browser", std::move(browser_metadata));
    }

    if (request.client_metadata().has_device()) {
      base::Value::Dict device_metadata;
      const auto& device = request.client_metadata().device();
      device_metadata.Set("dm_token", device.dm_token());
      device_metadata.Set("client_id", device.client_id());
      device_metadata.Set("os_version", device.os_version());
      device_metadata.Set("os_platform", device.os_platform());
      device_metadata.Set("name", device.name());
      metadata.Set("device", std::move(device_metadata));
    }

    if (request.client_metadata().has_profile()) {
      base::Value::Dict profile_metadata;
      const auto& profile = request.client_metadata().profile();
      profile_metadata.Set("dm_token", profile.dm_token());
      profile_metadata.Set("gaia_email", profile.gaia_email());
      profile_metadata.Set("profile_path", profile.profile_path());
      profile_metadata.Set("profile_name", profile.profile_name());
      profile_metadata.Set("client_id", profile.client_id());
      metadata.Set("profile", std::move(profile_metadata));
    }

    request_dict.Set("client_metadata", std::move(metadata));
  }

  base::Value::List tags;
  for (const std::string& tag : request.tags())
    tags.Append(tag);
  request_dict.Set("tags", std::move(tags));
  request_dict.Set("request_token", request.request_token());
  request_dict.Set("access_token", access_token_truncated);
  request_dict.Set("upload_info", upload_info);
  request_dict.Set(
      "is_chrome_os_managed_guest_session",
      request.client_metadata().is_chrome_os_managed_guest_session());
  request_dict.Set("upload_url", upload_url);

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(request_dict);
  return request_serialized;
}

std::string SerializeContentAnalysisResponse(
    const enterprise_connectors::ContentAnalysisResponse& response) {
  base::Value::Dict response_dict;

  response_dict.Set("token", response.request_token());

  base::Value::List result_values;
  for (const auto& result : response.results()) {
    base::Value::Dict result_value;
    result_value.Set(
        "status",
        enterprise_connectors::ContentAnalysisResponse_Result_Status_Name(
            result.status()));
    result_value.Set("tag", result.tag());

    base::Value::List triggered_rules;
    for (const auto& rule : result.triggered_rules()) {
      base::Value::Dict rule_value;

      rule_value.Set(
          "action",
          enterprise_connectors::
              ContentAnalysisResponse_Result_TriggeredRule_Action_Name(
                  rule.action()));
      rule_value.Set("rule_name", rule.rule_name());
      rule_value.Set("rule_id", rule.rule_id());
      rule_value.Set("url_category", rule.url_category());

      if (rule.has_custom_rule_message()) {
        base::Value::List message_segments;
        for (const auto& segment :
             rule.custom_rule_message().message_segments()) {
          base::Value::Dict segment_value;
          if (segment.has_text()) {
            segment_value.Set("text", segment.text());
          }
          if (segment.has_link()) {
            segment_value.Set("link", segment.link());
          }
          message_segments.Append(std::move(segment_value));
        }
        rule_value.Set("message_segments", std::move(message_segments));
      }
      triggered_rules.Append(std::move(rule_value));
    }
    result_value.Set("triggered_rules", std::move(triggered_rules));
    result_values.Append(std::move(result_value));
  }
  response_dict.Set("results", std::move(result_values));

  std::string response_serialized;
  JSONStringValueSerializer serializer(&response_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(response_dict);
  return response_serialized;
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

#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
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
  html_source->AddResourcePath("safe_browsing.css", IDR_SAFE_BROWSING_CSS);
  html_source->AddResourcePath("safe_browsing.js", IDR_SAFE_BROWSING_JS);
  html_source->SetDefaultResource(IDR_SAFE_BROWSING_HTML);

  // Static types
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

SafeBrowsingUI::~SafeBrowsingUI() {}

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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), GetFeatureStatusList());
}

void SafeBrowsingUIHandler::GetPrefs(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id),
                            safe_browsing::GetSafeBrowsingPreferencesList(
                                user_prefs::UserPrefs::Get(browser_context_)));
}

void SafeBrowsingUIHandler::GetPolicies(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id),
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
  response.Append(cookie);
  response.Append(time);

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), response);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), saved_passwords);
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
  std::string callback_id = args[0].GetString();

  ResolveJavascriptCallback(base::Value(callback_id), database_manager_info);
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

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(url_and_result);
  return request_serialized;
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), urls_checked_value);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), cdrs_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), cdrs_received);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), cprs_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), cprs_received);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), sent_reports);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), sent_reports);
}

void SafeBrowsingUIHandler::GetPGEvents(const base::Value::List& args) {
  const std::vector<sync_pb::UserEventSpecifics>& events =
      WebUIInfoSingleton::GetInstance()->pg_event_log();

  base::Value::List events_sent;

  for (const sync_pb::UserEventSpecifics& event : events)
    events_sent.Append(SerializePGEvent(event));

  AllowJavascript();
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), events_sent);
}

void SafeBrowsingUIHandler::GetSecurityEvents(const base::Value::List& args) {
  const std::vector<sync_pb::GaiaPasswordReuse>& events =
      WebUIInfoSingleton::GetInstance()->security_event_log();

  base::Value::List events_sent;

  for (const sync_pb::GaiaPasswordReuse& event : events)
    events_sent.Append(SerializeSecurityEvent(event));

  AllowJavascript();
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), events_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), pings_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), responses_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), pings_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), responses_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), pings_sent);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), responses_sent);
}

void SafeBrowsingUIHandler::GetReferrerChain(const base::Value::List& args) {
  DCHECK_GE(args.size(), 2U);
  std::string url_string = args[1].GetString();

  ReferrerChainProvider* provider =
      WebUIInfoSingleton::GetInstance()->GetReferrerChainProvider(
          browser_context_);

  std::string callback_id = args[0].GetString();

  if (!provider) {
    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(""));
    return;
  }

  ReferrerChain referrer_chain;
  provider->IdentifyReferrerChainByEventURL(
      GURL(url_string), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 2, &referrer_chain);

  base::Value::List referrer_list;
  for (const ReferrerChainEntry& entry : referrer_chain) {
    referrer_list.Append(SerializeReferrer(entry));
  }

  std::string referrer_chain_serialized;
  JSONStringValueSerializer serializer(&referrer_chain_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(referrer_list);

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(referrer_chain_serialized));
}

#if BUILDFLAG(IS_ANDROID)
void SafeBrowsingUIHandler::GetReferringAppInfo(const base::Value::List& args) {
  base::Value::Dict referring_app_value;
  ReferringAppInfo info =
      WebUIInfoSingleton::GetInstance()->GetReferringAppInfo(
          web_ui()->GetWebContents());
  referring_app_value = SerializeReferringAppInfo(info);

  std::string referring_app_serialized;
  JSONStringValueSerializer serializer(&referring_app_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(referring_app_value);

  AllowJavascript();
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(referring_app_serialized));
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), reporting_events);
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
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), messages_received);
}

void SafeBrowsingUIHandler::GetDeepScans(const base::Value::List& args) {
  base::Value::List pings_sent;
#if BUILDFLAG(FULL_SAFE_BROWSING)
  for (const auto& token_and_data :
       WebUIInfoSingleton::GetInstance()->deep_scan_requests()) {
    pings_sent.Append(SerializeDeepScanDebugData(token_and_data.first,
                                                 token_and_data.second));
  }
#endif

  AllowJavascript();
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), pings_sent);
}

base::Value::Dict SafeBrowsingUIHandler::GetFormattedTailoredVerdictOverride() {
  base::Value::Dict override_dict;
#if BUILDFLAG(FULL_SAFE_BROWSING)
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
                      SerializeTailoredVerdict(*override_data.override_value));
  }
#endif
  return override_dict;
}

void SafeBrowsingUIHandler::SetTailoredVerdictOverride(
    const base::Value::List& args) {
  DCHECK_GE(args.size(), 2U);
#if BUILDFLAG(FULL_SAFE_BROWSING)
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

  const base::Value::List* adjustments = input.FindList("adjustments");
  CHECK(adjustments);
  for (const base::Value& adjustment : *adjustments) {
    if (adjustment.GetString() == "ADJUSTMENT_UNSPECIFIED") {
      tv.add_adjustments(
          ClientDownloadResponse::TailoredVerdict::ADJUSTMENT_UNSPECIFIED);
    } else if (adjustment.GetString() == "ACCOUNT_INFO_STRING") {
      tv.add_adjustments(
          ClientDownloadResponse::TailoredVerdict::ACCOUNT_INFO_STRING);
    }
  }

  WebUIInfoSingleton::GetInstance()->SetTailoredVerdictOverride(std::move(tv),
                                                                this);
#endif

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::GetTailoredVerdictOverride(
    const base::Value::List& args) {
  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ClearTailoredVerdictOverride(
    const base::Value::List& args) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  WebUIInfoSingleton::GetInstance()->ClearTailoredVerdictOverride();
#endif

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ResolveTailoredVerdictOverrideCallback(
    const std::string& callback_id) {
  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id),
                            GetFormattedTailoredVerdictOverride());
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

#if BUILDFLAG(FULL_SAFE_BROWSING)
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

CrSBLogMessage::CrSBLogMessage() {}

CrSBLogMessage::~CrSBLogMessage() {
  WebUIInfoSingleton::GetInstance()->LogMessage(stream_.str());
  DLOG(WARNING) << stream_.str();
}

}  // namespace safe_browsing
