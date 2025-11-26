// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton.h"

#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#include "components/sync/protocol/user_event_specifics.pb.h"

namespace safe_browsing {
WebUIInfoSingleton::WebUIInfoSingleton() = default;

WebUIInfoSingleton::~WebUIInfoSingleton() = default;

bool WebUIInfoSingleton::HasListener() {
  return has_test_listener_ || !webui_instances_.empty();
}

void WebUIInfoSingleton::AddToDownloadUrlsChecked(const std::vector<GURL>& urls,
                                                  DownloadCheckResult result) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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
  web_ui::ClientPhishingRequestAndToken ping(
      std::move(*client_phishing_request), std::move(token));
  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       webui_instances_) {
    webui_listener->NotifyClientPhishingRequestJsListener(ping);
  }
  client_phishing_requests_sent_.emplace_back(std::move(ping));
}

void WebUIInfoSingleton::ClearClientPhishingRequestsSent() {
  std::vector<web_ui::ClientPhishingRequestAndToken>().swap(
      client_phishing_requests_sent_);
}

void WebUIInfoSingleton::AddToClientPhishingResponsesReceived(
    std::unique_ptr<ClientPhishingResponse> client_phishing_response) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  web_ui::LoginReputationClientRequestAndToken ping(request, oauth_token);

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       webui_instances_) {
    webui_listener->NotifyPGResponseJsListener(token, response);
  }

  pg_responses_[token] = response;
}

void WebUIInfoSingleton::ClearPGPings() {
  std::vector<web_ui::LoginReputationClientRequestAndToken>().swap(pg_pings_);
  std::map<int, LoginReputationClientResponse>().swap(pg_responses_);
}

int WebUIInfoSingleton::AddToURTLookupPings(const RTLookupRequest& request,
                                            const std::string& oauth_token) {
  if (!HasListener()) {
    return -1;
  }

  web_ui::URTLookupRequest ping(request, oauth_token);

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       webui_instances_) {
    webui_listener->NotifyURTLookupResponseJsListener(token, response);
  }

  urt_lookup_responses_[token] = response;
}

void WebUIInfoSingleton::ClearURTLookupPings() {
  std::vector<web_ui::URTLookupRequest>().swap(urt_lookup_pings_);
  std::map<int, RTLookupResponse>().swap(urt_lookup_responses_);
}

std::optional<int> WebUIInfoSingleton::AddToHPRTLookupPings(
    V5::SearchHashesRequest* inner_request,
    std::string relay_url_spec,
    std::string ohttp_key) {
  if (!HasListener()) {
    return std::nullopt;
  }
  web_ui::HPRTLookupRequest request(*inner_request, std::move(relay_url_spec),
                                    std::move(ohttp_key));
  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       webui_instances_) {
    webui_listener->NotifyHPRTLookupResponseJsListener(token, *response);
  }

  hprt_lookup_responses_[token] = *response;
}

void WebUIInfoSingleton::ClearHPRTLookupPings() {
  std::vector<web_ui::HPRTLookupRequest>().swap(hprt_lookup_pings_);
  std::map<int, V5::SearchHashesResponse>().swap(hprt_lookup_responses_);
}

void WebUIInfoSingleton::ClearLogMessages() {
  std::vector<std::pair<base::Time, std::string>>().swap(log_messages_);
}

void WebUIInfoSingleton::AddToReportingEvents(
    const ::chrome::cros::reporting::proto::UploadEventsRequest& event,
    const base::Value::Dict& result) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       webui_instances_) {
    webui_listener->NotifyReportingEventJsListener(event, result);
  }

  upload_event_requests_.emplace_back(std::move(event), result.Clone());
}

// TODO(crbug.com/443997643): Delete when
// UploadRealtimeReportingEventsUsingProto is cleaned up.
void WebUIInfoSingleton::AddToReportingEvents(const base::Value::Dict& event) {
  if (!HasListener()) {
    return;
  }

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       webui_instances_) {
    webui_listener->NotifyReportingEventJsListener(event);
  }

  reporting_events_.emplace_back(event.Clone());
}

void WebUIInfoSingleton::ClearReportingEvents() {
  std::vector<base::Value::Dict>().swap(reporting_events_);
  std::vector<std::pair<::chrome::cros::reporting::proto::UploadEventsRequest,
                        base::Value::Dict>>()
      .swap(upload_event_requests_);
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
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

  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       webui_instances_) {
    webui_listener->NotifyDeepScanJsListener(token, deep_scan_requests_[token]);
  }
}

void WebUIInfoSingleton::ClearDeepScans() {
  base::flat_map<std::string, web_ui::DeepScanDebugData>().swap(
      deep_scan_requests_);
}

void WebUIInfoSingleton::SetTailoredVerdictOverride(
    ClientDownloadResponse::TailoredVerdict new_value,
    const WebUIInfoSingletonEventObserver* new_source) {
  tailored_verdict_override_.Set(std::move(new_value), new_source);

  // Notify other listeners of the change. The source itself is notified by the
  // caller.
  for (WebUIInfoSingletonEventObserver* listener : webui_instances()) {
    if (!tailored_verdict_override_.IsFromSource(listener)) {
      listener->NotifyTailoredVerdictOverrideJsListener();
    }
  }
}

void WebUIInfoSingleton::ClearTailoredVerdictOverride() {
  tailored_verdict_override_.Clear();

  // Notify other listeners of the change. The source itself is notified by the
  // caller.
  for (WebUIInfoSingletonEventObserver* listener : webui_instances()) {
    if (!tailored_verdict_override_.IsFromSource(listener)) {
      listener->NotifyTailoredVerdictOverrideJsListener();
    }
  }
}
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

void WebUIInfoSingleton::RegisterWebUIInstance(
    WebUIInfoSingletonEventObserver* webui) {
  webui_instances_.push_back(webui);
}

void WebUIInfoSingleton::UnregisterWebUIInstance(
    WebUIInfoSingletonEventObserver* webui) {
  std::erase(webui_instances_, webui);

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  // Notify other WebUIs that the source of the tailored verdict override is
  // going away.
  if (tailored_verdict_override_.IsFromSource(webui)) {
    tailored_verdict_override_.Clear();
    for (WebUIInfoSingletonEventObserver* listener : webui_instances()) {
      listener->NotifyTailoredVerdictOverrideJsListener();
    }
  }
#endif

  MaybeClearData();
}

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

}  // namespace safe_browsing
