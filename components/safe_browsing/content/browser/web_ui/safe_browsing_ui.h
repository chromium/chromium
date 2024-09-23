// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/url_realtime_mechanism.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/enterprise/common/proto/connectors.pb.h"
#endif

namespace safe_browsing {
class WebUIInfoSingleton;
class ReferrerChainProvider;
class SafeBrowsingUIHandler;

#if BUILDFLAG(FULL_SAFE_BROWSING)
struct DeepScanDebugData {
  DeepScanDebugData();
  DeepScanDebugData(const DeepScanDebugData&);
  ~DeepScanDebugData();

  base::Time request_time;
  std::optional<enterprise_connectors::ContentAnalysisRequest> request;
  bool per_profile_request;
  std::string access_token_truncated;
  std::string upload_info;
  std::string upload_url;

  base::Time response_time;
  std::string response_status;
  std::optional<enterprise_connectors::ContentAnalysisResponse> response;
};

// Local override of a download TailoredVerdict.
struct TailoredVerdictOverrideData {
  // Identifies the SafeBrowsingUIHandler it was set from, it is derived from
  // a SafeBrowsingUIHandler* pointer but is only used in comparison and never
  // dereferenced, to avoid dangling pointer.
  using SourceId = std::uintptr_t;

  TailoredVerdictOverrideData();
  TailoredVerdictOverrideData(const TailoredVerdictOverrideData&) = delete;
  ~TailoredVerdictOverrideData();

  void Set(ClientDownloadResponse::TailoredVerdict new_value,
           const SafeBrowsingUIHandler* new_source);
  bool IsFromSource(const SafeBrowsingUIHandler* maybe_source) const;
  void Clear();

  std::optional<ClientDownloadResponse::TailoredVerdict> override_value;
  SourceId source = 0u;
};
#endif

// The struct to combine a PhishGuard request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct LoginReputationClientRequestAndToken {
  LoginReputationClientRequest request;
  std::string token;
};

// The struct to combine a URL real time lookup request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct URTLookupRequest {
  RTLookupRequest request;
  std::string token;
};

// Combines the inner request (SearchHashesRequest) sent to Safe Browsing with
// other details about the outer request (the relay URL + the OHTTP key used
// for encryption). All are displayed on chrome://safe-browsing.
struct HPRTLookupRequest {
  V5::SearchHashesRequest inner_request;
  std::string relay_url_spec;
  std::string ohttp_key;
};

// The struct to combine a client-side phishing request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct ClientPhishingRequestAndToken {
  ClientPhishingRequest request;
  std::string token;
};

// Provides access to local state preferences.
class SafeBrowsingLocalStateDelegate {
 public:
  SafeBrowsingLocalStateDelegate() = default;
  virtual ~SafeBrowsingLocalStateDelegate() = default;
  SafeBrowsingLocalStateDelegate(const SafeBrowsingLocalStateDelegate&) =
      delete;
  SafeBrowsingLocalStateDelegate& operator=(
      const SafeBrowsingLocalStateDelegate&) = delete;
  explicit SafeBrowsingLocalStateDelegate(content::WebUI* web_ui) {}
  // Returns the local state preference service.
  virtual PrefService* GetLocalState() = 0;
};

class SafeBrowsingUIHandler : public content::WebUIMessageHandler {
 public:
  SafeBrowsingUIHandler(
      content::BrowserContext* context,
      std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate);

  SafeBrowsingUIHandler(const SafeBrowsingUIHandler&) = delete;
  SafeBrowsingUIHandler& operator=(const SafeBrowsingUIHandler&) = delete;

  ~SafeBrowsingUIHandler() override;

  // Callback when Javascript becomes allowed in the WebUI.
  void OnJavascriptAllowed() override;

  // Callback when Javascript becomes disallowed in the WebUI.
  void OnJavascriptDisallowed() override;

  // Get the experiments that are currently enabled per Chrome instance.
  void GetExperiments(const base::Value::List& args);

  // Get the Safe Browsing related preferences for the current user.
  void GetPrefs(const base::Value::List& args);

  // Get the Safe Browsing related policies for the current user.
  void GetPolicies(const base::Value::List& args);

  // Get the Safe Browsing cookie.
  void GetCookie(const base::Value::List& args);

  // Get the current captured passwords.
  void GetSavedPasswords(const base::Value::List& args);

  // Get the information related to the Safe Browsing database and full hash
  // cache.
  void GetDatabaseManagerInfo(const base::Value::List& args);

  // Get the download URLs that have been checked since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetDownloadUrlsChecked(const base::Value::List& args);

  // Get the ClientDownloadRequests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSentClientDownloadRequests(const base::Value::List& args);

  // Get the ClientDownloadReponses that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetReceivedClientDownloadResponses(const base::Value::List& args);

  // Get the ClientPhishingRequests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSentClientPhishingRequests(const base::Value::List& args);

  // Get the ClientPhishingResponses that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetReceivedClientPhishingResponses(const base::Value::List& args);

  // Get the ThreatDetails that have been collected since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetSentCSBRRs(const base::Value::List& args);

  // Get the HitReports that have been collected since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetSentHitReports(const base::Value::List& args);

  // Get the PhishGuard events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetPGEvents(const base::Value::List& args);

  // Get the Security events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSecurityEvents(const base::Value::List& args);

  // Get the PhishGuard pings that have been sent since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetPGPings(const base::Value::List& args);

  // Get the PhishGuard responses that have been received since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetPGResponses(const base::Value::List& args);

  // Get the URL real time lookup pings that have been sent since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetURTLookupPings(const base::Value::List& args);

  // Get the URL real time lookup responses that have been received since the
  // oldest currently open chrome://safe-browsing tab was opened.
  void GetURTLookupResponses(const base::Value::List& args);

  // Get the hash-prefix real-time lookup pings that have been sent since the
  // oldest currently open chrome://safe-browsing tab was opened.
  void GetHPRTLookupPings(const base::Value::List& args);

  // Get the hash-prefix real-time lookup responses that have been received
  // since the oldest currently open chrome://safe-browsing tab was opened.
  void GetHPRTLookupResponses(const base::Value::List& args);

  // Get the current referrer chain for a given URL.
  void GetReferrerChain(const base::Value::List& args);

#if BUILDFLAG(IS_ANDROID)
  // Get the referring app info that launches Chrome on Android. Always set to
  // null if it's called from platforms other than Android.
  void GetReferringAppInfo(const base::Value::List& args);
#endif

  // Get the list of log messages that have been received since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetLogMessages(const base::Value::List& args);

  // Get the reporting events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetReportingEvents(const base::Value::List& args);

  // Get the deep scanning requests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetDeepScans(const base::Value::List& args);

  // Get the most recently set tailored verdict override, if its setting
  // chrome://safe-browsing tab has not been closed.
  void GetTailoredVerdictOverride(const base::Value::List& args);

  // Sets the tailored verdict override from args.
  void SetTailoredVerdictOverride(const base::Value::List& args);

  // Clears the current tailored verdict override.
  void ClearTailoredVerdictOverride(const base::Value::List& args);

  // Register callbacks for WebUI messages.
  void RegisterMessages() override;

  // Sets the WebUI for testing
  void SetWebUIForTesting(content::WebUI* web_ui);

 private:
  friend class WebUIInfoSingleton;

  // Called when a new download URL is checked while one or more WebUI tabs are
  // open.
  void NotifyDownloadUrlCheckedJsListener(const std::vector<GURL>& gurl,
                                          DownloadCheckResult result);

  // Called when any new ClientDownloadRequest messages are sent while one or
  // more WebUI tabs are open.
  void NotifyClientDownloadRequestJsListener(
      ClientDownloadRequest* client_download_request);

  // Called when any new ClientDownloadResponse messages are received while one
  // or more WebUI tabs are open.
  void NotifyClientDownloadResponseJsListener(
      ClientDownloadResponse* client_download_response);

  // Called when any new ClientPhishingRequest messages are sent (potentially
  // with token in header) while one or more WebUI tabs are open.
  void NotifyClientPhishingRequestJsListener(
      const ClientPhishingRequestAndToken& client_phishing_request);

  // Called when any new ClientPhishingResponse messages are received while one
  // or more WebUI tabs are open.
  void NotifyClientPhishingResponseJsListener(
      ClientPhishingResponse* client_phishing_response);

  // Get the new ThreatDetails messages sent from ThreatDetails when a ping is
  // sent, while one or more WebUI tabs are opened.
  void NotifyCSBRRJsListener(ClientSafeBrowsingReportRequest* csbrr);

  // Get the new HitReport messages sent from PingManager when a ping is
  // sent, while one or more WebUI tabs are opened.
  void NotifyHitReportJsListener(HitReport* hit_report);

  // Called when any new PhishGuard events are sent while one or more WebUI tabs
  // are open.
  void NotifyPGEventJsListener(const sync_pb::UserEventSpecifics& event);

  // Called when any new Security events are sent while one or more WebUI tabs
  // are open.
  void NotifySecurityEventJsListener(const sync_pb::GaiaPasswordReuse& event);

  // Called when any new PhishGuard pings are sent while one or more WebUI tabs
  // are open.
  void NotifyPGPingJsListener(
      int token,
      const LoginReputationClientRequestAndToken& request);

  // Called when any new PhishGuard responses are received while one or more
  // WebUI tabs are open.
  void NotifyPGResponseJsListener(
      int token,
      const LoginReputationClientResponse& response);

  // Called when any new URL real time lookup pings are sent while one or more
  // WebUI tabs are open.
  void NotifyURTLookupPingJsListener(int token,
                                     const URTLookupRequest& request);

  // Called when any new URL real time lookup responses are received while one
  // or more WebUI tabs are open.
  void NotifyURTLookupResponseJsListener(int token,
                                         const RTLookupResponse& response);

  // Called when any new hash-prefix real-time lookup pings are sent while one
  // or more WebUI tabs are open.
  void NotifyHPRTLookupPingJsListener(int token,
                                      const HPRTLookupRequest& request);

  // Called when any new hash-prefix real-time lookup responses are received
  // while one or more WebUI tabs are open.
  void NotifyHPRTLookupResponseJsListener(
      int token,
      const V5::SearchHashesResponse& response);

  // Called when any new log messages are received while one or more WebUI tabs
  // are open.
  void NotifyLogMessageJsListener(const base::Time& timestamp,
                                  const std::string& message);

  // Called when any new reporting events are sent while one or more WebUI tabs
  // are open.
  void NotifyReportingEventJsListener(const base::Value::Dict& event);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Called when any deep scans are updated while one or more WebUI
  // tabs are open.
  void NotifyDeepScanJsListener(const std::string& token,
                                const DeepScanDebugData& request);
#endif

  // Gets the tailored verdict override in a format for displaying.
  base::Value::Dict GetFormattedTailoredVerdictOverride();

  // Notifies the WebUI instance that a change in tailored verdict override
  // occurred.
  void NotifyTailoredVerdictOverrideJsListener();

  // Sends formatted tailored verdict override information to the WebUI.
  void ResolveTailoredVerdictOverrideCallback(const std::string& callback_id);

  // Callback when the CookieManager has returned the cookie.
  void OnGetCookie(const std::string& callback_id,
                   const std::vector<net::CanonicalCookie>& cookies);

  raw_ptr<content::BrowserContext> browser_context_;

  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;

  // List that keeps all the WebUI listener objects.
  static std::vector<SafeBrowsingUIHandler*> webui_list_;

  // Returns PrefService for local state.
  std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate_;

  base::WeakPtrFactory<SafeBrowsingUIHandler> weak_factory_{this};
};

// The WebUI for chrome://safe-browsing
class SafeBrowsingUI : public content::WebUIController {
 public:
  SafeBrowsingUI(content::WebUI* web_ui,
                 std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate);

  SafeBrowsingUI(const SafeBrowsingUI&) = delete;
  SafeBrowsingUI& operator=(const SafeBrowsingUI&) = delete;

  ~SafeBrowsingUI() override;
};

class WebUIInfoSingleton : public RealTimeUrlLookupServiceBase::WebUIDelegate,
                           public PingManager::WebUIDelegate,
                           public HashRealTimeService::WebUIDelegate {
 public:
  WebUIInfoSingleton();
  ~WebUIInfoSingleton() override;

  static WebUIInfoSingleton* GetInstance();

  WebUIInfoSingleton(const WebUIInfoSingleton&) = delete;
  WebUIInfoSingleton& operator=(const WebUIInfoSingleton&) = delete;

  // Returns true when there is a listening chrome://safe-browsing tab.
  static bool HasListener();

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
                   const std::string oauth_token);

  // Add the new response to |pg_responses_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToPGResponses(int token,
                        const LoginReputationClientResponse& response);

  // Clear the list of sent PhishGuard pings and responses.
  void ClearPGPings();

  // UrlRealTimeMechanism::WebUIDelegate:
  int AddToURTLookupPings(const RTLookupRequest request,
                          const std::string oauth_token) override;
  void AddToURTLookupResponses(int token,
                               const RTLookupResponse response) override;

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
  void LogMessage(const std::string& message);

  // Clear the log messages.
  void ClearLogMessages();

  // Notify listeners of changes to the log messages. Static to avoid this being
  // called after the destruction of the WebUIInfoSingleton
  static void NotifyLogMessageListeners(const base::Time& timestamp,
                                        const std::string& message);

  // Add the reporting event to |reporting_events_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToReportingEvents(const base::Value::Dict& event);

  // Clear |reporting_events_|.
  void ClearReportingEvents();

#if BUILDFLAG(FULL_SAFE_BROWSING)
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
      const SafeBrowsingUIHandler* new_source);

  // Clears any registered tailored verdict override.
  void ClearTailoredVerdictOverride();
#endif

  // Register the new WebUI listener object.
  void RegisterWebUIInstance(SafeBrowsingUIHandler* webui);

  // Unregister the WebUI listener object, and clean the list of reports, if
  // this is last listener.
  void UnregisterWebUIInstance(SafeBrowsingUIHandler* webui);

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
  const std::vector<ClientPhishingRequestAndToken>&
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
  const std::vector<raw_ptr<SafeBrowsingUIHandler, VectorExperimental>>&
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
  const std::vector<LoginReputationClientRequestAndToken>& pg_pings() const {
    return pg_pings_;
  }

  // Get the list of PhishGuard pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::map<int, LoginReputationClientResponse>& pg_responses() const {
    return pg_responses_;
  }

  // Get the list of URL real time lookup pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<URTLookupRequest>& urt_lookup_pings() const {
    return urt_lookup_pings_;
  }

  // Get the list of URL real time lookup pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::map<int, RTLookupResponse>& urt_lookup_responses() const {
    return urt_lookup_responses_;
  }

  // Get the list of hash-prefix real-time lookup pings since the oldest
  // currently open chrome://safe-browsing tab was opened.
  const std::vector<HPRTLookupRequest>& hprt_lookup_pings() const {
    return hprt_lookup_pings_;
  }

  // Get the list of hash-prefix real-time lookup pings since the oldest
  // currently open chrome://safe-browsing tab was opened.
  const std::map<int, V5::SearchHashesResponse>& hprt_lookup_responses() const {
    return hprt_lookup_responses_;
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Get the collection of deep scanning requests since the oldest currently
  // open chrome://safe-browsing tab was opened. Returns a map from a unique
  // token to the request proto.
  const base::flat_map<std::string, DeepScanDebugData>& deep_scan_requests()
      const {
    return deep_scan_requests_;
  }

  // Gets the currently registered override data.
  const TailoredVerdictOverrideData& tailored_verdict_override() const {
    return tailored_verdict_override_;
  }
#endif

  const std::vector<std::pair<base::Time, std::string>>& log_messages() {
    return log_messages_;
  }

  const std::vector<base::Value::Dict>& reporting_events() {
    return reporting_events_;
  }

  mojo::Remote<network::mojom::CookieManager> GetCookieManager(
      content::BrowserContext* browser_context);

  ReferrerChainProvider* GetReferrerChainProvider(
      content::BrowserContext* browser_context);

#if BUILDFLAG(IS_ANDROID)
  ReferringAppInfo GetReferringAppInfo(content::WebContents* web_contents);
#endif

  void set_safe_browsing_service(SafeBrowsingServiceInterface* sb_service) {
    sb_service_ = sb_service;
  }

  void AddListenerForTesting() { has_test_listener_ = true; }

  void ClearListenerForTesting();

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
  // open chrome://safe-browsing tab was opened. "ClientDownloadReponse" cannot
  // be const, due to being used by functions that call AllowJavascript(), which
  // is not marked const.
  std::vector<std::unique_ptr<ClientDownloadResponse>>
      client_download_responses_received_;

  // List of ClientPhishingRequests and tokens sent since since the oldest
  // currently open chrome://safe-browsing tab was opened.
  // "ClientPhishingRequests" cannot be const, due to being used by functions
  // that call AllowJavascript(), which is not marked const.
  std::vector<ClientPhishingRequestAndToken> client_phishing_requests_sent_;

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
  std::vector<LoginReputationClientRequestAndToken> pg_pings_;

  // List of PhishGuard responses received since the oldest currently open
  // chrome://safe-browsing tab was opened. Keyed by the index of the
  // corresponding request in |pg_pings_|.
  std::map<int, LoginReputationClientResponse> pg_responses_;

  // List of URL real time lookup pings sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<URTLookupRequest> urt_lookup_pings_;

  // List of URL real time lookup responses received since the oldest currently
  // open chrome://safe-browsing tab was opened. Keyed by the index of the
  // corresponding request in |rt_lookup_pings_|.
  std::map<int, RTLookupResponse> urt_lookup_responses_;

  // List of hash-prefix real-time lookup pings sent since the oldest currently
  // open chrome://safe-browsing tab was opened.
  std::vector<HPRTLookupRequest> hprt_lookup_pings_;

  // List of hash-prefix real-time lookup responses received since the oldest
  // currently open chrome://safe-browsing tab was opened. Keyed by the index of
  // the corresponding request in |hprt_lookup_pings_|.
  std::map<int, V5::SearchHashesResponse> hprt_lookup_responses_;

  // List of WebUI listener objects. "SafeBrowsingUIHandler*" cannot be const,
  // due to being used by functions that call AllowJavascript(), which is not
  // marked const.
  std::vector<raw_ptr<SafeBrowsingUIHandler, VectorExperimental>>
      webui_instances_;

  // List of messages logged since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<std::pair<base::Time, std::string>> log_messages_;

  // List of reporting events logged since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<base::Value::Dict> reporting_events_;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Map of deep scan requests sent since the oldest currently open
  // chrome://safe-browsing tab was opened. Maps from the unique token per
  // request to the data about the request.
  base::flat_map<std::string, DeepScanDebugData> deep_scan_requests_;

  // Local override of download TailoredVerdict.
  TailoredVerdictOverrideData tailored_verdict_override_;
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

  // The Safe Browsing service.
  raw_ptr<SafeBrowsingServiceInterface> sb_service_ = nullptr;

  // Whether there is a test listener.
  bool has_test_listener_ = false;
};

// Used for streaming messages to the WebUIInfoSingleton. Collects streamed
// messages, then sends them to the WebUIInfoSingleton when destroyed. Intended
// to be used in CRSBLOG macro.
class CrSBLogMessage {
 public:
  CrSBLogMessage();
  ~CrSBLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  std::ostringstream stream_;
};

// Used to consume a stream so that we don't even evaluate the streamed data if
// there are no chrome://safe-browsing tabs open.
class CrSBLogVoidify {
 public:
  CrSBLogVoidify() = default;

  // This has to be an operator with a precedence lower than <<,
  // but higher than ?:
  void operator&(std::ostream&) {}
};

#define CRSBLOG                                         \
  (!::safe_browsing::WebUIInfoSingleton::HasListener()) \
      ? static_cast<void>(0)                            \
      : ::safe_browsing::CrSBLogVoidify() &             \
            ::safe_browsing::CrSBLogMessage().stream()

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_
