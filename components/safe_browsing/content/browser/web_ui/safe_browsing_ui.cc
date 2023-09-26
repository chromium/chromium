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
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
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
  return base::Singleton<WebUIInfoSingleton>::get();
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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyDownloadUrlCheckedJsListener(urls, result);
  download_urls_checked_.emplace_back(urls, result);
}

void WebUIInfoSingleton::AddToClientDownloadRequestsSent(
    std::unique_ptr<ClientDownloadRequest> client_download_request) {
  if (!HasListener())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyClientDownloadRequestJsListener(
        client_download_request.get());
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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyClientDownloadResponseJsListener(
        client_download_response.get());
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
  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyClientPhishingRequestJsListener(ping);
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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyClientPhishingResponseJsListener(
        client_phishing_response.get());
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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyCSBRRJsListener(csbrr.get());
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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyHitReportJsListener(hit_report.get());
  hit_reports_sent_.push_back(std::move(hit_report));
}

void WebUIInfoSingleton::ClearHitReportsSent() {
  std::vector<std::unique_ptr<HitReport>>().swap(hit_reports_sent_);
}

void WebUIInfoSingleton::AddToPGEvents(
    const sync_pb::UserEventSpecifics& event) {
  if (!HasListener())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyPGEventJsListener(event);

  pg_event_log_.push_back(event);
}

void WebUIInfoSingleton::ClearPGEvents() {
  std::vector<sync_pb::UserEventSpecifics>().swap(pg_event_log_);
}

void WebUIInfoSingleton::AddToSecurityEvents(
    const sync_pb::GaiaPasswordReuse& event) {
  if (!HasListener())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifySecurityEventJsListener(event);

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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyPGPingJsListener(pg_pings_.size(), ping);

  pg_pings_.push_back(ping);

  return pg_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToPGResponses(
    int token,
    const LoginReputationClientResponse& response) {
  if (!HasListener())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyPGResponseJsListener(token, response);

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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyURTLookupPingJsListener(urt_lookup_pings_.size(),
                                                  ping);

  urt_lookup_pings_.push_back(ping);

  return urt_lookup_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToURTLookupResponses(
    int token,
    const RTLookupResponse response) {
  if (!HasListener())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyURTLookupResponseJsListener(token, response);

  urt_lookup_responses_[token] = response;
}

void WebUIInfoSingleton::ClearURTLookupPings() {
  std::vector<URTLookupRequest>().swap(urt_lookup_pings_);
  std::map<int, RTLookupResponse>().swap(urt_lookup_responses_);
}

absl::optional<int> WebUIInfoSingleton::AddToHPRTLookupPings(
    V5::SearchHashesRequest* inner_request,
    std::string relay_url_spec,
    std::string ohttp_key) {
  if (!HasListener()) {
    return absl::nullopt;
  }
  HPRTLookupRequest request = {.inner_request = *inner_request,
                               .relay_url_spec = relay_url_spec,
                               .ohttp_key = ohttp_key};
  for (auto* webui_listener : webui_instances_) {
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

  for (auto* webui_listener : webui_instances_) {
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

  for (auto* webui_listener : web_ui_info->webui_instances())
    webui_listener->NotifyLogMessageJsListener(timestamp, message);
}

void WebUIInfoSingleton::AddToReportingEvents(const base::Value::Dict& event) {
  if (!HasListener())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyReportingEventJsListener(event);

  reporting_events_.emplace_back(event.Clone());
}

void WebUIInfoSingleton::ClearReportingEvents() {
  std::vector<base::Value::Dict>().swap(reporting_events_);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
void WebUIInfoSingleton::AddToDeepScanRequests(
    bool per_profile_request,
    const enterprise_connectors::ContentAnalysisRequest& request) {
  if (!HasListener())
    return;

  // Only update the request time the first time we see a token.
  if (deep_scan_requests_.find(request.request_token()) ==
      deep_scan_requests_.end()) {
    deep_scan_requests_[request.request_token()].request_time =
        base::Time::Now();
  }

  deep_scan_requests_[request.request_token()].per_profile_request =
      per_profile_request;
  deep_scan_requests_[request.request_token()].request = request;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyDeepScanJsListener(
        request.request_token(), deep_scan_requests_[request.request_token()]);
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

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyDeepScanJsListener(token, deep_scan_requests_[token]);
}

void WebUIInfoSingleton::ClearDeepScans() {
  base::flat_map<std::string, DeepScanDebugData>().swap(deep_scan_requests_);
}
#endif
void WebUIInfoSingleton::RegisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  webui_instances_.push_back(webui);
}

void WebUIInfoSingleton::UnregisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  base::Erase(webui_instances_, webui);
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
LoginReputationClientRequest::ReferringAppInfo
WebUIInfoSingleton::GetReferringAppInfo(content::WebContents* web_contents) {
  return sb_service_ ? sb_service_->GetReferringAppInfo(web_contents)
                     : LoginReputationClientRequest::ReferringAppInfo();
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
#endif
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
DeepScanDebugData::DeepScanDebugData() = default;
DeepScanDebugData::DeepScanDebugData(const DeepScanDebugData&) = default;
DeepScanDebugData::~DeepScanDebugData() = default;
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
    std::string state_base64;
    base::Base64Encode(store_info.state(), &state_base64);
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

std::string SerializeClientSideDetectionType(ClientSideDetectionType csd_type) {
  switch (csd_type) {
    case ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED:
      return "CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED";
    case ClientSideDetectionType::FORCE_REQUEST:
      return "FORCE_REQUEST";
  }
  return "UNKNOWN_ENUM_SPECIFIED";
}

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

  std::string user_population;
  switch (population.user_population()) {
    case ChromeUserPopulation::UNKNOWN_USER_POPULATION:
      user_population = "UNKNOWN_USER_POPULATION";
      break;
    case ChromeUserPopulation::SAFE_BROWSING:
      user_population = "SAFE_BROWSING";
      break;
    case ChromeUserPopulation::EXTENDED_REPORTING:
      user_population = "EXTENDED_REPORTING";
      break;
    case ChromeUserPopulation::ENHANCED_PROTECTION:
      user_population = "ENHANCED_PROTECTION";
      break;
  }
  population_dict.Set("user_population", user_population);

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
    std::string management_status;
    switch (population.profile_management_status()) {
      case ChromeUserPopulation::UNKNOWN:
        management_status = "UNKNOWN";
        break;
      case ChromeUserPopulation::UNAVAILABLE:
        management_status = "UNAVAILABLE";
        break;
      case ChromeUserPopulation::NOT_MANAGED:
        management_status = "NOT_MANAGED";
        break;
      case ChromeUserPopulation::ENTERPRISE_MANAGED:
        management_status = "ENTERPRISE_MANAGED";
        break;
    }
    population_dict.Set("profile_management_status", management_status);
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
    std::string token_source;
    switch (token.token_source()) {
      case ChromeUserPopulation::PageLoadToken::SOURCE_UNSPECIFIED:
        token_source = "SOURCE_UNSPECIFIED";
        break;
      case ChromeUserPopulation::PageLoadToken::CLIENT_GENERATION:
        token_source = "CLIENT_GENERATION";
        break;
    }
    token_dict.Set("token_source", token_source);
    token_dict.Set("token_time_msec",
                   static_cast<double>(token.token_time_msec()));

    std::string token_base64;
    base::Base64Encode(token.token_value(), &token_base64);
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

  std::string url_type;
  switch (referrer.type()) {
    case ReferrerChainEntry::EVENT_URL:
      url_type = "EVENT_URL";
      break;
    case ReferrerChainEntry::LANDING_PAGE:
      url_type = "LANDING_PAGE";
      break;
    case ReferrerChainEntry::LANDING_REFERRER:
      url_type = "LANDING_REFERRER";
      break;
    case ReferrerChainEntry::CLIENT_REDIRECT:
      url_type = "CLIENT_REDIRECT";
      break;
    case ReferrerChainEntry::DEPRECATED_SERVER_REDIRECT:
      url_type = "DEPRECATED_SERVER_REDIRECT";
      break;
    case ReferrerChainEntry::RECENT_NAVIGATION:
      url_type = "RECENT_NAVIGATION";
      break;
    case ReferrerChainEntry::REFERRER:
      url_type = "REFERRER";
      break;
  }
  referrer_dict.Set("type", url_type);

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

  std::string navigation_initiation;
  switch (referrer.navigation_initiation()) {
    case ReferrerChainEntry::UNDEFINED:
      navigation_initiation = "UNDEFINED";
      break;
    case ReferrerChainEntry::BROWSER_INITIATED:
      navigation_initiation = "BROWSER_INITIATED";
      break;
    case ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE:
      navigation_initiation = "RENDERER_INITIATED_WITHOUT_USER_GESTURE";
      break;
    case ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE:
      navigation_initiation = "RENDERER_INITIATED_WITH_USER_GESTURE";
      break;
    case ReferrerChainEntry::COPY_PASTE_USER_INITIATED:
      navigation_initiation = "COPY_PASTE_USER_INITIATED";
      break;
    case ReferrerChainEntry::NOTIFICATION_INITIATED:
      navigation_initiation = "NOTIFICATION_INITIATED";
  }
  referrer_dict.Set("navigation_initiation", navigation_initiation);

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
    dict.Set("digests.sha256", base::HexEncode(sha256.c_str(), sha256.size()));
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
        dict_archived_binary.Set(
            "digests.sha256", base::HexEncode(sha256.c_str(), sha256.size()));
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

  if (cdr.has_document_summary()) {
    base::Value::Dict dict_document_summary;
    auto document_summary = cdr.document_summary();
    if (document_summary.has_metadata()) {
      base::Value::Dict dict_document_metadata;
      dict_document_metadata.Set("contains_macros",
                                 document_summary.metadata().contains_macros());
      dict_document_summary.Set("metadata", std::move(dict_document_metadata));
    }

    if (document_summary.has_processing_info()) {
      base::Value::Dict dict_document_processing_info;
      auto processing_info = document_summary.processing_info();
      if (processing_info.has_maldoca_error_type()) {
        dict_document_processing_info.Set("maldoca_error_type",
                                          processing_info.maldoca_error_type());
      }
      if (!processing_info.maldoca_error_message().empty()) {
        dict_document_processing_info.Set(
            "maldoca_error_message", processing_info.maldoca_error_message());
      }
      dict_document_processing_info.Set(
          "processing_successful", processing_info.processing_successful());
      dict_document_summary.Set("processing_info",
                                std::move(dict_document_processing_info));
    }

    dict.Set("document_summary", std::move(dict_document_summary));
  }

  if (cdr.has_archive_summary()) {
    base::Value::Dict dict_archive_summary;
    auto archive_summary = cdr.archive_summary();
    dict_archive_summary.Set("parser_status", archive_summary.parser_status());
    dict_archive_summary.Set("file_count", archive_summary.file_count());
    dict_archive_summary.Set("directory_count",
                             archive_summary.directory_count());
    dict.Set("archive_summary", std::move(dict_archive_summary));
  }

  if (cdr.has_tailored_info()) {
    base::Value::Dict dict_tailored_info;
    auto tailored_info = cdr.tailored_info();
    dict_tailored_info.Set("version", tailored_info.version());
    dict.Set("tailored_info", std::move(dict_tailored_info));
  }

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(dict);
  return request_serialized;
}

std::string ClientDownloadResponseVerdictToString(
    const ClientDownloadResponse::Verdict& verdict) {
  switch (verdict) {
    case ClientDownloadResponse::SAFE:
      return "SAFE";
    case ClientDownloadResponse::DANGEROUS:
      return "DANGEROUS";
    case ClientDownloadResponse::UNCOMMON:
      return "UNCOMMON";
    case ClientDownloadResponse::POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    case ClientDownloadResponse::DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case ClientDownloadResponse::UNKNOWN:
      return "UNKNOWN";
    case ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE:
      return "DANGEROUS_ACCOUNT_COMPROMISE";
  }
}

std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr) {
  base::Value::Dict dict;

  if (cdr.has_verdict()) {
    dict.Set("verdict", ClientDownloadResponseVerdictToString(cdr.verdict()));
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
    base::Value::Dict dict_tailored_verdict;
    auto tailored_verdict = cdr.tailored_verdict();
    std::string tailored_verdict_type;
    switch (tailored_verdict.tailored_verdict_type()) {
      case ClientDownloadResponse::TailoredVerdict::VERDICT_TYPE_UNSPECIFIED:
        tailored_verdict_type = "VERDICT_TYPE_UNSPECIFIED";
        break;
      case ClientDownloadResponse::TailoredVerdict::COOKIE_THEFT:
        tailored_verdict_type = "COOKIE_THEFT";
        break;
      case ClientDownloadResponse::TailoredVerdict::SUSPICIOUS_ARCHIVE:
        tailored_verdict_type = "SUSPICIOUS_ARCHIVE";
        break;
    }
    dict_tailored_verdict.Set("tailored_verdict_type", tailored_verdict_type);
    base::Value::List adjustments;
    for (const auto& adjustment : tailored_verdict.adjustments()) {
      std::string adjustment_string;
      switch (adjustment) {
        case ClientDownloadResponse::TailoredVerdict::ADJUSTMENT_UNSPECIFIED:
          adjustment_string = "ADJUSTMENT_UNSPECIFIED";
          break;
        case ClientDownloadResponse::TailoredVerdict::ACCOUNT_INFO_STRING:
          adjustment_string = "ACCOUNT_INFO_STRING";
          break;
      }
      adjustments.Append(adjustment_string);
    }
    dict_tailored_verdict.Set("adjustments", std::move(adjustments));
    dict.Set("tailored_verdict", std::move(dict_tailored_verdict));
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
    dict.Set(
        "client_side_detection_type",
        SerializeClientSideDetectionType(cpr.client_side_detection_type()));
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
  std::string url_api_type;
  switch (client_properties.url_api_type()) {
    case ClientSafeBrowsingReportRequest::
        SAFE_BROWSING_URL_API_TYPE_UNSPECIFIED:
      url_api_type = "SAFE_BROWSING_URL_API_TYPE_UNSPECIFIED";
      break;
    case ClientSafeBrowsingReportRequest::PVER4_NATIVE:
      url_api_type = "PVER4_NATIVE";
      break;
    case ClientSafeBrowsingReportRequest::ANDROID_SAFETYNET:
      url_api_type = "ANDROID_SAFETYNET";
      break;
    case ClientSafeBrowsingReportRequest::REAL_TIME:
      url_api_type = "REAL_TIME";
      break;
    case ClientSafeBrowsingReportRequest::PVER5_NATIVE_REAL_TIME:
      url_api_type = "PVER5_NATIVE_REAL_TIME";
      break;
    case ClientSafeBrowsingReportRequest::ANDROID_SAFEBROWSING_REAL_TIME:
      url_api_type = "ANDROID_SAFEBROWSING_REAL_TIME";
      break;
    case ClientSafeBrowsingReportRequest::PVER3_NATIVE:
    case ClientSafeBrowsingReportRequest::FLYWHEEL:
      NOTREACHED();
      url_api_type = "";
      break;
  }
  client_properties_dict.Set("url_api_type", url_api_type);
  return client_properties_dict;
}

base::Value::Dict SerializeHashRealTimeExperimentDetails(
    const ClientSafeBrowsingReportRequest::HashRealTimeExperimentDetails&
        details) {
  base::Value::Dict dict;
  auto serialize_threat_type =
      [](ClientSafeBrowsingReportRequest::HashRealTimeExperimentDetails::
             ExperimentThreatType threat_type) -> std::string {
    switch (threat_type) {
      case ClientSafeBrowsingReportRequest::HashRealTimeExperimentDetails::
          SAFE_OR_OTHER:
        return "SAFE_OR_OTHER";
      case ClientSafeBrowsingReportRequest::HashRealTimeExperimentDetails::
          MALWARE:
        return "MALWARE";
      case ClientSafeBrowsingReportRequest::HashRealTimeExperimentDetails::
          PHISHING:
        return "PHISHING";
      case ClientSafeBrowsingReportRequest::HashRealTimeExperimentDetails::
          UNWANTED:
        return "UNWANTED";
      case ClientSafeBrowsingReportRequest::HashRealTimeExperimentDetails::
          BILLING:
        return "BILLING";
      default:
        NOTREACHED();
        return "NOTREACHED";
    }
  };
  // Hash database details:
  dict.Set("hash_database_threat_type",
           serialize_threat_type(details.hash_database_threat_type()));
  // Hash real-time details:
  dict.Set(
      "hash_realtime_threat_type",
      serialize_threat_type(details.hash_realtime_details().threat_type()));
  dict.Set("hash_realtime_matched_global_cache",
           details.hash_realtime_details().matched_global_cache());
  if (details.hash_realtime_details()
          .has_locally_cached_results_threat_type()) {
    dict.Set("hash_realtime_locally_cached_results_threat_type",
             serialize_threat_type(details.hash_realtime_details()
                                       .locally_cached_results_threat_type()));
  }
  // URL real-time details:
  dict.Set("url_realtime_threat_type",
           serialize_threat_type(details.url_realtime_details().threat_type()));
  dict.Set("url_realtime_matched_global_cache",
           details.url_realtime_details().matched_global_cache());
  if (details.url_realtime_details().has_locally_cached_results_threat_type()) {
    dict.Set("url_realtime_locally_cached_results_threat_type",
             serialize_threat_type(details.url_realtime_details()
                                       .locally_cached_results_threat_type()));
  }
  return dict;
}

std::string UrlRequestDestinationToString(
    const ClientSafeBrowsingReportRequest::UrlRequestDestination&
        request_destination) {
  switch (request_destination) {
    case ClientSafeBrowsingReportRequest::REQUEST_DESTINATION_UNSPECIFIED:
      return "REQUEST_DESTINATION_UNSPECIFIED";
    case ClientSafeBrowsingReportRequest::EMPTY:
      return "EMPTY";
    case ClientSafeBrowsingReportRequest::AUDIO:
      return "AUDIO";
    case ClientSafeBrowsingReportRequest::AUDIO_WORKLET:
      return "AUDIO_WORKLET";
    case ClientSafeBrowsingReportRequest::DOCUMENT:
      return "DOCUMENT";
    case ClientSafeBrowsingReportRequest::EMBED:
      return "EMBED";
    case ClientSafeBrowsingReportRequest::FONT:
      return "FONT";
    case ClientSafeBrowsingReportRequest::FRAME:
      return "FRAME";
    case ClientSafeBrowsingReportRequest::IFRAME:
      return "IFRAME";
    case ClientSafeBrowsingReportRequest::IMAGE:
      return "IMAGE";
    case ClientSafeBrowsingReportRequest::MANIFEST:
      return "MANIFEST";
    case ClientSafeBrowsingReportRequest::OBJECT:
      return "OBJECT";
    case ClientSafeBrowsingReportRequest::PAINT_WORKLET:
      return "PAINT_WORKLET";
    case ClientSafeBrowsingReportRequest::REPORT:
      return "REPORT";
    case ClientSafeBrowsingReportRequest::SCRIPT:
      return "SCRIPT";
    case ClientSafeBrowsingReportRequest::SERVICE_WORKER:
      return "SERVICE_WORKER";
    case ClientSafeBrowsingReportRequest::SHARED_WORKER:
      return "SHARED_WORKER";
    case ClientSafeBrowsingReportRequest::STYLE:
      return "STYLE";
    case ClientSafeBrowsingReportRequest::TRACK:
      return "TRACK";
    case ClientSafeBrowsingReportRequest::VIDEO:
      return "VIDEO";
    case ClientSafeBrowsingReportRequest::WEB_BUNDLE:
      return "WEB_BUNDLE";
    case ClientSafeBrowsingReportRequest::WORKER:
      return "WORKER";
    case ClientSafeBrowsingReportRequest::XSLT:
      return "XSLT";
    case ClientSafeBrowsingReportRequest::FENCED_FRAME:
      return "FENCED_FRAME";
    case ClientSafeBrowsingReportRequest::WEB_IDENTITY:
      return "WEB_IDENTITY";
    case ClientSafeBrowsingReportRequest::DICTIONARY:
      return "DICTIONARY";
  }
}

base::Value::Dict SerializeDownloadWarningAction(
    const ClientSafeBrowsingReportRequest::DownloadWarningAction&
        download_warning_action) {
  base::Value::Dict action_dict;
  std::string surface;
  switch (download_warning_action.surface()) {
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::
        SURFACE_UNSPECIFIED:
      surface = "SURFACE_UNSPECIFIED";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::
        BUBBLE_MAINPAGE:
      surface = "BUBBLE_MAINPAGE";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::BUBBLE_SUBPAGE:
      surface = "BUBBLE_SUBPAGE";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::DOWNLOADS_PAGE:
      surface = "DOWNLOADS_PAGE";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::
        DOWNLOAD_PROMPT:
      surface = "DOWNLOAD_PROMPT";
      break;
  }
  action_dict.Set("surface", surface);
  std::string action;
  switch (download_warning_action.action()) {
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::
        ACTION_UNSPECIFIED:
      action = "ACTION_UNSPECIFIED";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::PROCEED:
      action = "PROCEED";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::DISCARD:
      action = "DISCARD";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::KEEP:
      action = "KEEP";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::CLOSE:
      action = "CLOSE";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::CANCEL:
      action = "CANCEL";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::DISMISS:
      action = "DISMISS";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::BACK:
      action = "BACK";
      break;
    case ClientSafeBrowsingReportRequest::DownloadWarningAction::OPEN_SUBPAGE:
      action = "OPEN_SUBPAGE";
      break;
  }
  action_dict.Set("action", action);
  action_dict.Set("is_terminal_action",
                  download_warning_action.is_terminal_action());
  action_dict.Set("interval_msec",
                  static_cast<double>(download_warning_action.interval_msec()));
  return action_dict;
}

std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report) {
  base::Value::Dict report_request;
  if (report.has_type()) {
    std::string report_type;
    switch (report.type()) {
      case ClientSafeBrowsingReportRequest::UNKNOWN:
        report_type = "UNKNOWN";
        break;
      case ClientSafeBrowsingReportRequest::URL_PHISHING:
        report_type = "URL_PHISHING";
        break;
      case ClientSafeBrowsingReportRequest::URL_MALWARE:
        report_type = "URL_MALWARE";
        break;
      case ClientSafeBrowsingReportRequest::URL_UNWANTED:
        report_type = "URL_UNWANTED ";
        break;
      case ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING:
        report_type = "URL_CLIENT_SIDE_PHISHING";
        break;
      case ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_MALWARE:
        report_type = "URL_CLIENT_SIDE_MALWARE";
        break;
      case ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY:
        report_type = "DANGEROUS_DOWNLOAD_RECOVERY";
        break;
      case ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING:
        report_type = "DANGEROUS_DOWNLOAD_WARNING";
        break;
      case ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_BY_API:
        report_type = "DANGEROUS_DOWNLOAD_BY_API";
        break;
      case ClientSafeBrowsingReportRequest::URL_PASSWORD_PROTECTION_PHISHING:
        report_type = "URL_PASSWORD_PROTECTION_PHISHING";
        break;
      case ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED:
        report_type = "DANGEROUS_DOWNLOAD_OPENED";
        break;
      case ClientSafeBrowsingReportRequest::AD_SAMPLE:
        report_type = "AD_SAMPLE";
        break;
      case ClientSafeBrowsingReportRequest::URL_SUSPICIOUS:
        report_type = "URL_SUSPICIOUS";
        break;
      case ClientSafeBrowsingReportRequest::BILLING:
        report_type = "BILLING";
        break;
      case ClientSafeBrowsingReportRequest::APK_DOWNLOAD:
        report_type = "APK_DOWNLOAD";
        break;
      case ClientSafeBrowsingReportRequest::BLOCKED_AD_REDIRECT:
        report_type = "BLOCKED_AD_REDIRECT";
        break;
      case ClientSafeBrowsingReportRequest::BLOCKED_AD_POPUP:
        report_type = "BLOCKED_AD_POPUP";
        break;
      case ClientSafeBrowsingReportRequest::HASH_PREFIX_REAL_TIME_EXPERIMENT:
        report_type = "HASH_PREFIX_REAL_TIME_EXPERIMENT";
        break;
      case ClientSafeBrowsingReportRequest::PHISHY_SITE_INTERACTIONS:
        report_type = "PHISHY_SITE_INTERACTIONS";
        break;
    }
    report_request.Set("type", report_type);
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
    report_request.Set(
        "download_verdict",
        ClientDownloadResponseVerdictToString(report.download_verdict()));
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
  if (report.has_hash_real_time_experiment_details()) {
    report_request.Set("hash_real_time_experiment_details",
                       SerializeHashRealTimeExperimentDetails(
                           report.hash_real_time_experiment_details()));
  }
  if (report.has_url_request_destination()) {
    report_request.Set(
        "url_request_destination",
        UrlRequestDestinationToString(report.url_request_destination()));
  }
  std::string serialized;
  if (report.SerializeToString(&serialized)) {
    std::string base64_encoded;
    base::Base64Encode(serialized, &base64_encoded);
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
    case ThreatSource::REMOTE:
      threat_source = "REMOTE";
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

base::Value SerializeReuseLookup(
    const PasswordReuseLookup password_reuse_lookup) {
  std::string lookup_result;
  switch (password_reuse_lookup.lookup_result()) {
    case PasswordReuseLookup::UNSPECIFIED:
      lookup_result = "UNSPECIFIED";
      break;
    case PasswordReuseLookup::WHITELIST_HIT:
      lookup_result = "WHITELIST_HIT";
      break;
    case PasswordReuseLookup::CACHE_HIT:
      lookup_result = "CACHE_HIT";
      break;
    case PasswordReuseLookup::REQUEST_SUCCESS:
      lookup_result = "REQUEST_SUCCESS";
      break;
    case PasswordReuseLookup::REQUEST_FAILURE:
      lookup_result = "REQUEST_FAILURE";
      break;
    case PasswordReuseLookup::URL_UNSUPPORTED:
      lookup_result = "URL_UNSUPPORTED";
      break;
    case PasswordReuseLookup::ENTERPRISE_WHITELIST_HIT:
      lookup_result = "ENTERPRISE_WHITELIST_HIT";
      break;
    case PasswordReuseLookup::TURNED_OFF_BY_POLICY:
      lookup_result = "TURNED_OFF_BY_POLICY";
      break;
  }
  return base::Value(lookup_result);
}

base::Value SerializeVerdict(const PasswordReuseLookup password_reuse_lookup) {
  std::string verdict;
  switch (password_reuse_lookup.verdict()) {
    case PasswordReuseLookup::VERDICT_UNSPECIFIED:
      verdict = "VERDICT_UNSPECIFIED";
      break;
    case PasswordReuseLookup::SAFE:
      verdict = "SAFE";
      break;
    case PasswordReuseLookup::LOW_REPUTATION:
      verdict = "LOW_REPUTATION";
      break;
    case PasswordReuseLookup::PHISHING:
      verdict = "PHISHING";
      break;
  }
  return base::Value(verdict);
}

base::Value::Dict SerializePGEvent(const sync_pb::UserEventSpecifics& event) {
  base::Value::Dict result;

  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(event.event_time_usec()));
  result.Set("time", timestamp.ToJsTime());

  base::Value::Dict event_dict;

  // Nominally only one of the following if() statements would be true.
  // Note that top-level path is either password_captured, or one of the fields
  // under GaiaPasswordReuse (ie. we've flattened the namespace for simplicity).

  if (event.has_gaia_password_captured_event()) {
    std::string event_trigger;

    switch (event.gaia_password_captured_event().event_trigger()) {
      case PasswordCaptured::UNSPECIFIED:
        event_trigger = "UNSPECIFIED";
        break;
      case PasswordCaptured::USER_LOGGED_IN:
        event_trigger = "USER_LOGGED_IN";
        break;
      case PasswordCaptured::EXPIRED_28D_TIMER:
        event_trigger = "EXPIRED_28D_TIMER";
        break;
    }

    event_dict.SetByDottedPath("password_captured.event_trigger",
                               event_trigger);
  }

  GaiaPasswordReuse reuse = event.gaia_password_reuse_event();
  if (reuse.has_reuse_detected()) {
    event_dict.SetByDottedPath("reuse_detected.status.enabled",
                               reuse.reuse_detected().status().enabled());

    std::string reporting_population;
    switch (
        reuse.reuse_detected().status().safe_browsing_reporting_population()) {
      case PasswordReuseDetected::SafeBrowsingStatus::
          REPORTING_POPULATION_UNSPECIFIED:
        reporting_population = "REPORTING_POPULATION_UNSPECIFIED";
        break;
      case PasswordReuseDetected::SafeBrowsingStatus::NONE:
        reporting_population = "NONE";
        break;
      case PasswordReuseDetected::SafeBrowsingStatus::EXTENDED_REPORTING:
        reporting_population = "EXTENDED_REPORTING";
        break;
      case PasswordReuseDetected::SafeBrowsingStatus::SCOUT:
        reporting_population = "SCOUT";
        break;
    }
    event_dict.SetByDottedPath("reuse_detected.status.reporting_population",
                               reporting_population);
  }

  if (reuse.has_reuse_lookup()) {
    event_dict.SetByDottedPath("reuse_lookup.lookup_result",
                               SerializeReuseLookup(reuse.reuse_lookup()));
    event_dict.SetByDottedPath("reuse_lookup.verdict",
                               SerializeVerdict(reuse.reuse_lookup()));
    event_dict.SetByDottedPath("reuse_lookup.verdict_token",
                               reuse.reuse_lookup().verdict_token());
  }

  if (reuse.has_dialog_interaction()) {
    std::string interaction_result;
    switch (reuse.dialog_interaction().interaction_result()) {
      case PasswordReuseDialogInteraction::UNSPECIFIED:
        interaction_result = "UNSPECIFIED";
        break;
      case PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN:
        interaction_result = "WARNING_ACTION_TAKEN";
        break;
      case PasswordReuseDialogInteraction::WARNING_ACTION_IGNORED:
        interaction_result = "WARNING_ACTION_IGNORED";
        break;
      case PasswordReuseDialogInteraction::WARNING_UI_IGNORED:
        interaction_result = "WARNING_UI_IGNORED";
        break;
      case PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN_ON_SETTINGS:
        interaction_result = "WARNING_ACTION_TAKEN_ON_SETTINGS";
        break;
    }
    event_dict.SetByDottedPath("dialog_interaction.interaction_result",
                               interaction_result);
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
    event_dict.SetByDottedPath("reuse_lookup.lookup_result",
                               SerializeReuseLookup(event.reuse_lookup()));
    event_dict.SetByDottedPath("reuse_lookup.verdict",
                               SerializeVerdict(event.reuse_lookup()));
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

  std::string sync_account_type;
  switch (event.sync_account_type()) {
    case LoginReputationClientRequest::PasswordReuseEvent::NOT_SIGNED_IN:
      sync_account_type = "NOT_SIGNED_IN";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::GMAIL:
      sync_account_type = "GMAIL";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::GSUITE:
      sync_account_type = "GSUITE";
      break;
  }
  event_dict.Set("sync_account_type", sync_account_type);

  std::string reused_password_type;
  switch (event.reused_password_type()) {
    case LoginReputationClientRequest::PasswordReuseEvent::
        REUSED_PASSWORD_TYPE_UNKNOWN:
      reused_password_type = "REUSED_PASSWORD_TYPE_UNKNOWN";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::SAVED_PASSWORD:
      reused_password_type = "SAVED_PASSWORD";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::SIGN_IN_PASSWORD:
      reused_password_type = "SIGN_IN_PASSWORD";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::OTHER_GAIA_PASSWORD:
      reused_password_type = "OTHER_GAIA_PASSWORD";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::ENTERPRISE_PASSWORD:
      reused_password_type = "ENTERPRISE_PASSWORD";
      break;
  }
  event_dict.Set("reused_password_type", reused_password_type);

  std::string reused_password_account_type;
  switch (event.reused_password_account_type().account_type()) {
    case LoginReputationClientRequest::PasswordReuseEvent::
        ReusedPasswordAccountType::UNKNOWN:
      reused_password_account_type = "UNKNOWN";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::
        ReusedPasswordAccountType::GSUITE:
      reused_password_account_type = "GSUITE";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::
        ReusedPasswordAccountType::GMAIL:
      reused_password_account_type = "GMAIL";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::
        ReusedPasswordAccountType::NON_GAIA_ENTERPRISE:
      reused_password_account_type = "NON_GAIA_ENTERPRISE";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::
        ReusedPasswordAccountType::SAVED_PASSWORD:
      reused_password_account_type = "SAVED_PASSWORD";
      break;
  }
  event_dict.Set("reused_password_account_type", reused_password_account_type);
  event_dict.Set("is_account_syncing",
                 event.reused_password_account_type().is_account_syncing());

  return event_dict;
}

base::Value::Dict SerializeRTThreatInfo(
    const RTLookupResponse::ThreatInfo& threat_info) {
  base::Value::Dict threat_info_dict;
  std::string threat_type;
  switch (threat_info.threat_type()) {
    case RTLookupResponse::ThreatInfo::THREAT_TYPE_UNSPECIFIED:
      threat_type = "THREAT_TYPE_UNSPECIFIED";
      break;
    case RTLookupResponse::ThreatInfo::WEB_MALWARE:
      threat_type = "WEB_MALWARE";
      break;
    case RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING:
      threat_type = "SOCIAL_ENGINEERING";
      break;
    case RTLookupResponse::ThreatInfo::UNWANTED_SOFTWARE:
      threat_type = "UNWANTED_SOFTWARE";
      break;
    case RTLookupResponse::ThreatInfo::UNCLEAR_BILLING:
      threat_type = "UNCLEAR_BILLING";
      break;
    case RTLookupResponse::ThreatInfo::MANAGED_POLICY:
      threat_type = "MANAGED_POLICY";
      break;
  }
  threat_info_dict.Set("threat_type", threat_type);

  threat_info_dict.Set("cache_duration_sec",
                       static_cast<double>(threat_info.cache_duration_sec()));

  std::string verdict_type;
  switch (threat_info.verdict_type()) {
    case RTLookupResponse::ThreatInfo::VERDICT_TYPE_UNSPECIFIED:
      verdict_type = "VERDICT_TYPE_UNSPECIFIED";
      break;
    case RTLookupResponse::ThreatInfo::SAFE:
      verdict_type = "SAFE";
      break;
    case RTLookupResponse::ThreatInfo::SUSPICIOUS:
      verdict_type = "SUSPICIOUS";
      break;
    case RTLookupResponse::ThreatInfo::WARN:
      verdict_type = "WARN";
      break;
    case RTLookupResponse::ThreatInfo::DANGEROUS:
      verdict_type = "DANGEROUS";
      break;
  }
  threat_info_dict.Set("verdict_type", verdict_type);

  std::string cache_expression_match_type;
  switch (threat_info.cache_expression_match_type()) {
    case RTLookupResponse::ThreatInfo::MATCH_TYPE_UNSPECIFIED:
      cache_expression_match_type = "MATCH_TYPE_UNSPECIFIED";
      break;
    case RTLookupResponse::ThreatInfo::COVERING_MATCH:
      cache_expression_match_type = "COVERING_MATCH";
      break;
    case RTLookupResponse::ThreatInfo::EXACT_MATCH:
      cache_expression_match_type = "EXACT_MATCH";
      break;
  }

  threat_info_dict.Set("cache_expression_match_type",
                       cache_expression_match_type);
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

base::Value::Dict SerializeReferringAppInfo(
    const LoginReputationClientRequest::ReferringAppInfo& info) {
  base::Value::Dict dict;

  std::string source;
  switch (info.referring_app_source()) {
    case LoginReputationClientRequest::ReferringAppInfo::
        REFERRING_APP_SOURCE_UNSPECIFIED:
      source = "REFERRING_APP_SOURCE_UNSPECIFIED";
      break;
    case LoginReputationClientRequest::ReferringAppInfo::KNOWN_APP_ID:
      source = "KNOWN_APP_ID";
      break;
    case LoginReputationClientRequest::ReferringAppInfo::UNKNOWN_APP_ID:
      source = "UNKNOWN_APP_ID";
      break;
    case LoginReputationClientRequest::ReferringAppInfo::ACTIVITY_REFERRER:
      source = "ACTIVITY_REFERRER";
      break;
  }
  dict.Set("referring_app_source", source);
  dict.Set("referring_app_info", info.referring_app_name());

  return dict;
}

std::string SerializePGPing(
    const LoginReputationClientRequestAndToken& request_and_token) {
  base::Value::Dict request_dict;

  const LoginReputationClientRequest& request = request_and_token.request;

  request_dict.Set("page_url", request.page_url());

  std::string trigger_type;
  switch (request.trigger_type()) {
    case LoginReputationClientRequest::TRIGGER_TYPE_UNSPECIFIED:
      trigger_type = "TRIGGER_TYPE_UNSPECIFIED";
      break;
    case LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE:
      trigger_type = "UNFAMILIAR_LOGIN_PAGE";
      break;
    case LoginReputationClientRequest::PASSWORD_REUSE_EVENT:
      trigger_type = "PASSWORD_REUSE_EVENT";
      break;
  }
  request_dict.Set("trigger_type", trigger_type);

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

  request_dict.Set("scoped_oauth_token", request_and_token.token);

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(request_dict);
  return request_serialized;
}

std::string SerializePGResponse(const LoginReputationClientResponse& response) {
  base::Value::Dict response_dict;

  std::string verdict;
  switch (response.verdict_type()) {
    case LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED:
      verdict = "VERDICT_TYPE_UNSPECIFIED";
      break;
    case LoginReputationClientResponse::SAFE:
      verdict = "SAFE";
      break;
    case LoginReputationClientResponse::LOW_REPUTATION:
      verdict = "LOW_REPUTATION";
      break;
    case LoginReputationClientResponse::PHISHING:
      verdict = "PHISHING";
      break;
  }
  response_dict.Set("verdict_type", verdict);
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

  std::string lookupType;
  switch (request.lookup_type()) {
    case RTLookupRequest::LOOKUP_TYPE_UNSPECIFIED:
      lookupType = "LOOKUP_TYPE_UNSPECIFIED";
      break;
    case RTLookupRequest::NAVIGATION:
      lookupType = "NAVIGATION";
      break;
    case RTLookupRequest::DOWNLOAD:
      lookupType = "DOWNLOAD";
      break;
  }
  request_dict.Set("lookup_type", lookupType);

  request_dict.Set("version", request.version());

  std::string os;
  switch (request.os_type()) {
    case RTLookupRequest::OS_TYPE_UNSPECIFIED:
      DCHECK(false) << "RTLookupRequest::os_type is undefined.";
      os = "UNSPECIFIED";
      break;
    case RTLookupRequest::OS_TYPE_LINUX:
      os = "LINUX";
      break;
    case RTLookupRequest::OS_TYPE_WINDOWS:
      os = "WINDOWS";
      break;
    case RTLookupRequest::OS_TYPE_MAC:
      os = "MAC";
      break;
    case RTLookupRequest::OS_TYPE_ANDROID:
      os = "ANDROID";
      break;
    case RTLookupRequest::OS_TYPE_IOS:
      os = "IOS";
      break;
    case RTLookupRequest::OS_TYPE_CHROME_OS:
      os = "CHROME_OS";
      break;
    case RTLookupRequest::OS_TYPE_FUCHSIA:
      os = "FUCHSIA";
      break;
  }
  request_dict.Set("os", os);

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

std::string SerializeV5ThreatType(V5::ThreatType threat_type) {
  switch (threat_type) {
    case V5::THREAT_TYPE_UNSPECIFIED:
      return "THREAT_TYPE_UNSPECIFIED";
    case V5::MALWARE:
      return "MALWARE";
    case V5::SOCIAL_ENGINEERING:
      return "SOCIAL_ENGINEERING";
    case V5::UNWANTED_SOFTWARE:
      return "UNWANTED_SOFTWARE";
    case V5::POTENTIALLY_HARMFUL_APPLICATION:
      return "POTENTIALLY_HARMFUL_APPLICATION";
    case V5::API_ABUSE:
      return "API_ABUSE";
    case V5::TRICK_TO_BILL:
      return "TRICK_TO_BILL";
    case V5::ABUSIVE_EXPERIENCE_VIOLATION:
      return "ABUSIVE_EXPERIENCE_VIOLATION";
    case V5::BETTER_ADS_VIOLATION:
      return "BETTER_ADS_VIOLATION";
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      return "OTHER";
  }
}

std::string SerializeThreatAttribute(V5::ThreatAttribute attribute) {
  switch (attribute) {
    case V5::THREAT_ATTRIBUTE_UNSPECIFIED:
      return "THREAT_ATTRIBUTE_UNSPECIFIED";
    case V5::CANARY:
      return "CANARY";
    case V5::FRAME_ONLY:
      return "FRAME_ONLY";
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      return "OTHER";
  }
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
          "threat_type", SerializeV5ThreatType(full_hash_detail.threat_type()));
      // attributes
      base::Value::List attributes_list;
      for (auto i = 0; i < full_hash_detail.attributes_size(); ++i) {
        attributes_list.Append(
            SerializeThreatAttribute(full_hash_detail.attributes(i)));
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
  result.Set("time", timestamp.ToJsTime());
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
    const enterprise_connectors::ContentAnalysisRequest& request) {
  base::Value::Dict request_dict;

  request_dict.Set(per_profile_request ? "profile_token" : "device_token",
                   request.device_token());
  request_dict.Set("fcm_notification_token", request.fcm_notification_token());
  switch (request.analysis_connector()) {
    case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
      request_dict.Set("analysis_connector", "UNSPECIFIED");
      break;
    case enterprise_connectors::FILE_ATTACHED:
      request_dict.Set("analysis_connector", "FILE_ATTACHED");
      break;
    case enterprise_connectors::FILE_DOWNLOADED:
      request_dict.Set("analysis_connector", "FILE_DOWNLOADED");
      break;
    case enterprise_connectors::BULK_DATA_ENTRY:
      request_dict.Set("analysis_connector", "BULK_DATA_ENTRY");
      break;
    case enterprise_connectors::PRINT:
      request_dict.Set("analysis_connector", "PRINT");
      break;
    case enterprise_connectors::FILE_TRANSFER:
      request_dict.Set("analysis_connector", "FILE_TRANSFER");
      break;
  }

  switch (request.reason()) {
    case enterprise_connectors::ContentAnalysisRequest::UNKNOWN:
      request_dict.Set("reason", "UNKNOWN");
      break;
    case enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE:
      request_dict.Set("reason", "CLIPBOARD_PASTE");
      break;
    case enterprise_connectors::ContentAnalysisRequest::DRAG_AND_DROP:
      request_dict.Set("reason", "DRAG_AND_DROP");
      break;
    case enterprise_connectors::ContentAnalysisRequest::FILE_PICKER_DIALOG:
      request_dict.Set("reason", "FILE_PICKER_DIALOG");
      break;
    case enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT:
      request_dict.Set("reason", "PRINT_PREVIEW_PRINT");
      break;
    case enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT:
      request_dict.Set("reason", "SYSTEM_DIALOG_PRINT");
      break;
    case enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD:
      request_dict.Set("reason", "NORMAL_DOWNLOAD");
      break;
    case enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD:
      request_dict.Set("reason", "SAVE_AS_DOWNLOAD");
      break;
  }

  if (request.has_request_data()) {
    base::Value::Dict request_data;
    request_data.Set("url", request.request_data().url());
    request_data.Set("filename", request.request_data().filename());
    request_data.Set("digest", request.request_data().digest());
    if (request.request_data().has_csd()) {
      std::string csd_base64;
      base::Base64Encode(request.request_data().csd().SerializeAsString(),
                         &csd_base64);
      request_data.Set("csd", csd_base64);
    }
    request_data.Set("content_type", request.request_data().content_type());
    request_dict.Set("tab_url", request.request_data().tab_url());
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
    switch (result.status()) {
      case enterprise_connectors::ContentAnalysisResponse::Result::
          STATUS_UNKNOWN:
        result_value.Set("status", "STATUS_UNKNOWN");
        break;
      case enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS:
        result_value.Set("status", "SUCCESS");
        break;
      case enterprise_connectors::ContentAnalysisResponse::Result::FAILURE:
        result_value.Set("status", "FAILURE");
        break;
    }
    result_value.Set("tag", result.tag());

    base::Value::List triggered_rules;
    for (const auto& rule : result.triggered_rules()) {
      base::Value::Dict rule_value;

      switch (rule.action()) {
        case TriggeredRule::ACTION_UNSPECIFIED:
          rule_value.Set("action", "ACTION_UNSPECIFIED");
          break;
        case TriggeredRule::REPORT_ONLY:
          rule_value.Set("action", "REPORT_ONLY");
          break;
        case TriggeredRule::WARN:
          rule_value.Set("action", "WARN");
          break;
        case TriggeredRule::BLOCK:
          rule_value.Set("action", "BLOCK");
          break;
      }

      rule_value.Set("rule_name", rule.rule_name());
      rule_value.Set("rule_id", rule.rule_id());
      rule_value.Set("url_category", rule.url_category());
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
    value.Set("request_time", data.request_time.ToJsTime());
  }

  if (data.request.has_value()) {
    value.Set("request", SerializeContentAnalysisRequest(
                             data.per_profile_request, data.request.value()));
  }

  if (!data.response_time.is_null()) {
    value.Set("response_time", data.response_time.ToJsTime());
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

#endif
}  // namespace

SafeBrowsingUI::SafeBrowsingUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  // Set up the chrome://safe-browsing source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, safe_browsing::kChromeUISafeBrowsingHost);

  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  web_ui->AddMessageHandler(
      std::make_unique<SafeBrowsingUIHandler>(browser_context));

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

SafeBrowsingUIHandler::SafeBrowsingUIHandler(content::BrowserContext* context)
    : browser_context_(context) {}

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
    time = cookies[0].CreationDate().ToJsTime();
  }

  base::Value::List response;
  response.Append(cookie);
  response.Append(time);

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void SafeBrowsingUIHandler::GetSavedPasswords(const base::Value::List& args) {
  password_manager::HashPasswordManager hash_manager(
      user_prefs::UserPrefs::Get(browser_context_));

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

  switch (result) {
    case DownloadCheckResult::UNKNOWN:
      url_and_result.Set("result", "UNKNOWN");
      break;
    case DownloadCheckResult::SAFE:
      url_and_result.Set("result", "SAFE");
      break;
    case DownloadCheckResult::DANGEROUS:
      url_and_result.Set("result", "DANGEROUS");
      break;
    case DownloadCheckResult::UNCOMMON:
      url_and_result.Set("result", "UNCOMMON");
      break;
    case DownloadCheckResult::DANGEROUS_HOST:
      url_and_result.Set("result", "DANGEROUS_HOST");
      break;
    case DownloadCheckResult::POTENTIALLY_UNWANTED:
      url_and_result.Set("result", "POTENTIALLY_UNWANTED");
      break;
    case DownloadCheckResult::ALLOWLISTED_BY_POLICY:
      url_and_result.Set("result", "ALLOWLISTED_BY_POLICY");
      break;
    case DownloadCheckResult::ASYNC_SCANNING:
      url_and_result.Set("result", "ASYNC_SCANNING");
      break;
    case DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
      url_and_result.Set("result", "BLOCKED_PASSWORD_PROTECTED");
      break;
    case DownloadCheckResult::BLOCKED_TOO_LARGE:
      url_and_result.Set("result", "BLOCKED_TOO_LARGE");
      break;
    case DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
      url_and_result.Set("result", "SENSITIVE_CONTENT_WARNING");
      break;
    case DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
      url_and_result.Set("result", "SENSITIVE_CONTENT_BLOCK");
      break;
    case DownloadCheckResult::DEEP_SCANNED_SAFE:
      url_and_result.Set("result", "DEEP_SCANNED_SAFE");
      break;
    case DownloadCheckResult::PROMPT_FOR_SCANNING:
      url_and_result.Set("result", "PROMPT_FOR_SCANNING");
      break;
    case DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE:
      url_and_result.Set("result", "BLOCKED_UNSUPPORTED_FILE_TYPE");
      break;
    case DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE:
      url_and_result.Set("result", "DANGEROUS_ACCOUNT_COMPROMISE");
      break;
    case DownloadCheckResult::DEEP_SCANNED_FAILED:
      url_and_result.Set("result", "DEEP_SCANNED_FAILED");
      break;
  }

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
  LoginReputationClientRequest::ReferringAppInfo info =
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
