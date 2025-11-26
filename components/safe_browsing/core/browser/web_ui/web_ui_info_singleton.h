// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_H_

#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_util.h"

namespace sync_pb {
class GaiaPasswordReuse;
class UserEventSpecifics;
}  // namespace sync_pb

namespace safe_browsing {
class WebUIInfoSingletonEventObserver;

class WebUIInfoSingleton : public RealTimeUrlLookupServiceBase::WebUIDelegate,
                           public PingManager::WebUIDelegate,
                           public HashRealTimeService::WebUIDelegate {
 public:
  WebUIInfoSingleton();
  ~WebUIInfoSingleton() override;

  WebUIInfoSingleton(const WebUIInfoSingleton&) = delete;
  WebUIInfoSingleton& operator=(const WebUIInfoSingleton&) = delete;

  // Returns true when there is a listening chrome://safe-browsing tab.
  bool HasListener();

  // Add the new message in |download_urls_checked_| and send it to all
  // the open chrome://safe-browsing tabs.
  void AddToDownloadUrlsChecked(const std::vector<GURL>& urls,
                                DownloadCheckResult result);

  // Clear the list of the download URLs checked.
  void ClearDownloadUrlsChecked();

  // Add the new message in |client_download_requests_sent_| and send it to all
  // the open chrome://safe-browsing tabs.
  void AddToClientDownloadRequestsSent(
      std::unique_ptr<ClientDownloadRequest> report_request);

  // Clear the list of the sent ClientDownloadRequest messages.
  void ClearClientDownloadRequestsSent();

  // Add the new message in |client_download_responses_received_| and send it to
  // all the open chrome://safe-browsing tabs.
  void AddToClientDownloadResponsesReceived(
      std::unique_ptr<ClientDownloadResponse> response);

  // Clear the list of the received ClientDownloadResponse messages.
  void ClearClientDownloadResponsesReceived();

  // Add the new message and token in |client_phishing_requests_sent_| and send
  // it to all the open chrome://safe-browsing tabs.
  void AddToClientPhishingRequestsSent(
      std::unique_ptr<ClientPhishingRequest> client_phishing_request,
      std::string token);

  // Clear the list of the sent ClientPhishingRequest messages.
  void ClearClientPhishingRequestsSent();

  // Add the new message in |client_phishing_responses_received_| and send it to
  // all the open chrome://safe-browsing tabs.
  void AddToClientPhishingResponsesReceived(
      std::unique_ptr<ClientPhishingResponse> response);

  // Clear the list of the received ClientPhishingResponse messages.
  void ClearClientPhishingResponsesReceived();

  // PingManager::WebUIDelegate:
  // Add the new message in |csbrrs_sent_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToCSBRRsSent(
      std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr) override;
  // Add the new message in |hit_reports_sent_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToHitReportsSent(std::unique_ptr<HitReport> hit_report) override;

  // Clear the list of the sent ClientSafeBrowsingReportRequest messages.
  void ClearCSBRRsSent();

  void SetOnCSBRRLoggedCallbackForTesting(base::OnceClosure on_done);

  // Clear the list of the sent HitReport messages.
  void ClearHitReportsSent();

  // Add the new message in |pg_event_log_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToPGEvents(const sync_pb::UserEventSpecifics& event);

  // Clear the list of sent PhishGuard events.
  void ClearPGEvents();

  // Add the new message in |security_event_log_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToSecurityEvents(const sync_pb::GaiaPasswordReuse& event);

  // Clear the list of sent Security events.
  void ClearSecurityEvents();

  // Add the new ping (with oauth token) to |pg_pings_| and send it to all the
  // open chrome://safe-browsing tabs. Returns a token that can be used in
  // |AddToPGResponses| to correlate a ping and response.
  int AddToPGPings(const LoginReputationClientRequest& request,
                   const std::string& oauth_token);

  // Add the new response to |pg_responses_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToPGResponses(int token,
                        const LoginReputationClientResponse& response);

  // Clear the list of sent PhishGuard pings and responses.
  void ClearPGPings();

  // UrlRealTimeMechanism::WebUIDelegate:
  int AddToURTLookupPings(const RTLookupRequest& request,
                          const std::string& oauth_token) override;
  void AddToURTLookupResponses(int token,
                               const RTLookupResponse& response) override;

  // Clear the list of sent URT lookup pings and responses.
  void ClearURTLookupPings();

  // HashRealTimeService::WebUIDelegate:
  std::optional<int> AddToHPRTLookupPings(
      V5::SearchHashesRequest* inner_request,
      std::string relay_url_spec,
      std::string ohttp_key) override;
  void AddToHPRTLookupResponses(int token,
                                V5::SearchHashesResponse* response) override;

  // Clear the list of sent hash-prefix real-time pings and responses.
  void ClearHPRTLookupPings();

  // Log an arbitrary message. Frequently used for debugging.
  virtual void LogMessage(const std::string& message) = 0;

  // Clear the log messages.
  void ClearLogMessages();

  // Add the reporting event to |upload_event_requests_| and send it to all the
  // open chrome://safe-browsing tabs.
  void AddToReportingEvents(
      const ::chrome::cros::reporting::proto::UploadEventsRequest& event,
      const base::Value::Dict& result);

  // Add the reporting event to |reporting_events_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToReportingEvents(const base::Value::Dict& event);

  // Clear |reporting_events_| & |upload_event_requests_|.
  void ClearReportingEvents();

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  // Add the new request to |deep_scan_requests_| and send it to all the open
  // chrome://safe-browsing tabs. Uses |request.request_token()| as an
  // identifier that can be used in |AddToDeepScanResponses| to correlate a ping
  // and response.
  void AddToDeepScanRequests(
      bool per_profile_request,
      const std::string& access_token,
      const std::string& upload_info,
      const std::string& upload_url,
      const enterprise_connectors::ContentAnalysisRequest& request);

  // Add the new response to |deep_scan_requests_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToDeepScanResponses(
      const std::string& token,
      const std::string& status,
      const enterprise_connectors::ContentAnalysisResponse& response);

  // Clear the list of deep scan requests and responses.
  void ClearDeepScans();

  // Overwrites any existing override.
  void SetTailoredVerdictOverride(
      ClientDownloadResponse::TailoredVerdict new_value,
      const WebUIInfoSingletonEventObserver* new_source);

  // Clears any registered tailored verdict override.
  void ClearTailoredVerdictOverride();
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  // Register the new WebUI listener object.
  void RegisterWebUIInstance(WebUIInfoSingletonEventObserver* observer);

  // Unregister the WebUI listener object, and clean the list of reports, if
  // this is last listener.
  void UnregisterWebUIInstance(WebUIInfoSingletonEventObserver* observer);

  // Get the list of download URLs checked since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<std::pair<std::vector<GURL>, DownloadCheckResult>>&
  download_urls_checked() const {
    return download_urls_checked_;
  }

  // Get the list of the sent ClientDownloadRequests that have been collected
  // since the oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<ClientDownloadRequest>>&
  client_download_requests_sent() const {
    return client_download_requests_sent_;
  }

  // Get the list of the sent ClientDownloadResponses that have been collected
  // since the oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<ClientDownloadResponse>>&
  client_download_responses_received() const {
    return client_download_responses_received_;
  }

  // Get the list of the sent ClientPhishingRequestAndToken that have been
  // collected (potentially with token in header) since the oldest currently
  // open chrome://safe-browsing tab was opened.
  const std::vector<web_ui::ClientPhishingRequestAndToken>&
  client_phishing_requests_sent() const {
    return client_phishing_requests_sent_;
  }

  // Get the list of the sent ClientPhishingResponse that have been collected
  // since the oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<ClientPhishingResponse>>&
  client_phishing_responses_received() const {
    return client_phishing_responses_received_;
  }

  // Get the list of the sent CSBRR reports that have been collected since the
  // oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>&
  csbrrs_sent() const {
    return csbrrs_sent_;
  }

  // Get the list of the sent HitReports that have been collected since the
  // oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<HitReport>>& hit_reports_sent() const {
    return hit_reports_sent_;
  }

  // Get the list of WebUI listener objects.
  const std::vector<
      raw_ptr<WebUIInfoSingletonEventObserver, VectorExperimental>>&
  webui_instances() const {
    return webui_instances_;
  }

  // Get the list of PhishGuard events since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<sync_pb::UserEventSpecifics>& pg_event_log() const {
    return pg_event_log_;
  }

  // Get the list of Security events since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<sync_pb::GaiaPasswordReuse>& security_event_log() const {
    return security_event_log_;
  }

  // Get the list of PhishGuard pings and tokens since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<web_ui::LoginReputationClientRequestAndToken>& pg_pings()
      const {
    return pg_pings_;
  }

  // Get the list of PhishGuard pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::map<int, LoginReputationClientResponse>& pg_responses() const {
    return pg_responses_;
  }

  // Get the list of URL real time lookup pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<web_ui::URTLookupRequest>& urt_lookup_pings() const {
    return urt_lookup_pings_;
  }

  // Get the list of URL real time lookup pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::map<int, RTLookupResponse>& urt_lookup_responses() const {
    return urt_lookup_responses_;
  }

  // Get the list of hash-prefix real-time lookup pings since the oldest
  // currently open chrome://safe-browsing tab was opened.
  const std::vector<web_ui::HPRTLookupRequest>& hprt_lookup_pings() const {
    return hprt_lookup_pings_;
  }

  // Get the list of hash-prefix real-time lookup pings since the oldest
  // currently open chrome://safe-browsing tab was opened.
  const std::map<int, V5::SearchHashesResponse>& hprt_lookup_responses() const {
    return hprt_lookup_responses_;
  }

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  // Get the collection of deep scanning requests since the oldest currently
  // open chrome://safe-browsing tab was opened. Returns a map from a unique
  // token to the request proto.
  const base::flat_map<std::string, web_ui::DeepScanDebugData>&
  deep_scan_requests() const {
    return deep_scan_requests_;
  }

  // Gets the currently registered override data.
  const web_ui::TailoredVerdictOverrideData& tailored_verdict_override() const {
    return tailored_verdict_override_;
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  const std::vector<std::pair<base::Time, std::string>>& log_messages() {
    return log_messages_;
  }

  const std::vector<base::Value::Dict>& reporting_events() {
    return reporting_events_;
  }

  const std::vector<
      std::pair<::chrome::cros::reporting::proto::UploadEventsRequest,
                base::Value::Dict>>&
  upload_event_requests() {
    return upload_event_requests_;
  }

  void AddListenerForTesting() { has_test_listener_ = true; }

  void ClearListenerForTesting();

 protected:
  // List of messages logged since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<std::pair<base::Time, std::string>> log_messages_;

 private:
  void MaybeClearData();

  // List of download URLs checked since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<std::pair<std::vector<GURL>, DownloadCheckResult>>
      download_urls_checked_;

  // List of ClientDownloadRequests sent since since the oldest currently open
  // chrome://safe-browsing tab was opened.
  // "ClientDownloadRequests" cannot be const, due to being used by functions
  // that call AllowJavascript(), which is not marked const.
  std::vector<std::unique_ptr<ClientDownloadRequest>>
      client_download_requests_sent_;

  // List of ClientDownloadResponses received since since the oldest currently
  // open chrome://safe-browsing tab was opened. "ClientDownloadResponse" cannot
  // be const, due to being used by functions that call AllowJavascript(), which
  // is not marked const.
  std::vector<std::unique_ptr<ClientDownloadResponse>>
      client_download_responses_received_;

  // List of ClientPhishingRequests and tokens sent since since the oldest
  // currently open chrome://safe-browsing tab was opened.
  // "ClientPhishingRequests" cannot be const, due to being used by functions
  // that call AllowJavascript(), which is not marked const.
  std::vector<web_ui::ClientPhishingRequestAndToken>
      client_phishing_requests_sent_;

  // List of ClientPhishingResponses received since since the oldest currently
  // open chrome://safe-browsing tab was opened. "ClientPhishingResponse" cannot
  // be const, due to being used by functions that call AllowJavascript(), which
  // is not marked const.
  std::vector<std::unique_ptr<ClientPhishingResponse>>
      client_phishing_responses_received_;

  // List of CSBRRs sent since since the oldest currently open
  // chrome://safe-browsing tab was opened.
  // "ClientSafeBrowsingReportRequest" cannot be const, due to being used by
  // functions that call AllowJavascript(), which is not marked const.
  std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>> csbrrs_sent_;

  // Gets fired at the end of the AddToCSBRRsSent function. Only used for tests.
  base::OnceClosure on_csbrr_logged_for_testing_;

  // List of HitReports sent since since the oldest currently open
  // chrome://safe-browsing tab was opened.
  // "HitReport" cannot be const, due to being used by
  // functions that call AllowJavascript(), which is not marked const.
  std::vector<std::unique_ptr<HitReport>> hit_reports_sent_;

  // List of PhishGuard events sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<sync_pb::UserEventSpecifics> pg_event_log_;

  // List of Security events sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<sync_pb::GaiaPasswordReuse> security_event_log_;

  // List of PhishGuard pings and tokens sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<web_ui::LoginReputationClientRequestAndToken> pg_pings_;

  // List of PhishGuard responses received since the oldest currently open
  // chrome://safe-browsing tab was opened. Keyed by the index of the
  // corresponding request in |pg_pings_|.
  std::map<int, LoginReputationClientResponse> pg_responses_;

  // List of URL real time lookup pings sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<web_ui::URTLookupRequest> urt_lookup_pings_;

  // List of URL real time lookup responses received since the oldest currently
  // open chrome://safe-browsing tab was opened. Keyed by the index of the
  // corresponding request in |rt_lookup_pings_|.
  std::map<int, RTLookupResponse> urt_lookup_responses_;

  // List of hash-prefix real-time lookup pings sent since the oldest currently
  // open chrome://safe-browsing tab was opened.
  std::vector<web_ui::HPRTLookupRequest> hprt_lookup_pings_;

  // List of hash-prefix real-time lookup responses received since the oldest
  // currently open chrome://safe-browsing tab was opened. Keyed by the index of
  // the corresponding request in |hprt_lookup_pings_|.
  std::map<int, V5::SearchHashesResponse> hprt_lookup_responses_;

  // List of WebUI listener objects. "WebUIInfoSingletonEventObserver*" cannot
  // be const, due to being used by functions that call AllowJavascript(), which
  // is not marked const.
  std::vector<raw_ptr<WebUIInfoSingletonEventObserver, VectorExperimental>>
      webui_instances_;

  // List of reporting events logged since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<base::Value::Dict> reporting_events_;
  std::vector<std::pair<::chrome::cros::reporting::proto::UploadEventsRequest,
                        base::Value::Dict>>
      upload_event_requests_;

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  // Map of deep scan requests sent since the oldest currently open
  // chrome://safe-browsing tab was opened. Maps from the unique token per
  // request to the data about the request.
  base::flat_map<std::string, web_ui::DeepScanDebugData> deep_scan_requests_;

  // Local override of download TailoredVerdict.
  web_ui::TailoredVerdictOverrideData tailored_verdict_override_;
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  // Whether there is a test listener.
  bool has_test_listener_ = false;
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_H_
