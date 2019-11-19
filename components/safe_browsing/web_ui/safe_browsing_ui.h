// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_
#define COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_

#include "base/bind.h"
#include "base/macros.h"
#include "components/safe_browsing/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/safe_browsing/proto/realtimeapi.pb.h"
#include "components/safe_browsing/proto/webui.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace base {
class ListValue;
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace safe_browsing {
class WebUIInfoSingleton;
class ReferrerChainProvider;

class SafeBrowsingUIHandler : public content::WebUIMessageHandler {
 public:
  SafeBrowsingUIHandler(content::BrowserContext* context);
  ~SafeBrowsingUIHandler() override;

  // Callback when Javascript becomes allowed in the WebUI.
  void OnJavascriptAllowed() override;

  // Callback when Javascript becomes disallowed in the WebUI.
  void OnJavascriptDisallowed() override;

  // Get the experiments that are currently enabled per Chrome instance.
  void GetExperiments(const base::ListValue* args);

  // Get the Safe Browsing related preferences for the current user.
  void GetPrefs(const base::ListValue* args);

  // Get the Safe Browsing cookie.
  void GetCookie(const base::ListValue* args);

  // Get the current captured passwords.
  void GetSavedPasswords(const base::ListValue* args);

  // Get the information related to the Safe Browsing database and full hash
  // cache.
  void GetDatabaseManagerInfo(const base::ListValue* args);

  // Get the ClientDownloadRequests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSentClientDownloadRequests(const base::ListValue* args);

  // Get the ClientDownloadReponses that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetReceivedClientDownloadResponses(const base::ListValue* args);

  // Get the ThreatDetails that have been collected since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetSentCSBRRs(const base::ListValue* args);

  // Get the PhishGuard events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetPGEvents(const base::ListValue* args);

  // Get the Security events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSecurityEvents(const base::ListValue* args);

  // Get the PhishGuard pings that have been sent since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetPGPings(const base::ListValue* args);

  // Get the PhishGuard responses that have been received since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetPGResponses(const base::ListValue* args);

  // Get the real time lookup pings that have been sent since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetRTLookupPings(const base::ListValue* args);

  // Get the real time lookup responses that have been received since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetRTLookupResponses(const base::ListValue* args);

  // Get the current referrer chain for a given URL.
  void GetReferrerChain(const base::ListValue* args);

  // Get the list of log messages that have been received since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetLogMessages(const base::ListValue* args);

  // Register callbacks for WebUI messages.
  void RegisterMessages() override;

  // Sets the WebUI for testing
  void SetWebUIForTesting(content::WebUI* web_ui);

 private:
  friend class WebUIInfoSingleton;

  // Called when any new ClientDownloadRequest messages are sent while one or
  // more WebUI tabs are open.
  void NotifyClientDownloadRequestJsListener(
      ClientDownloadRequest* client_download_request);

  // Called when any new ClientDownloadResponse messages are received while one
  // or more WebUI tabs are open.
  void NotifyClientDownloadResponseJsListener(
      ClientDownloadResponse* client_download_response);

  // Get the new ThreatDetails messages sent from ThreatDetails when a ping is
  // sent, while one or more WebUI tabs are opened.
  void NotifyCSBRRJsListener(ClientSafeBrowsingReportRequest* csbrr);

  // Called when any new PhishGuard events are sent while one or more WebUI tabs
  // are open.
  void NotifyPGEventJsListener(const sync_pb::UserEventSpecifics& event);

  // Called when any new Security events are sent while one or more WebUI tabs
  // are open.
  void NotifySecurityEventJsListener(const sync_pb::GaiaPasswordReuse& event);

  // Called when any new PhishGuard pings are sent while one or more WebUI tabs
  // are open.
  void NotifyPGPingJsListener(int token,
                              const LoginReputationClientRequest& request);

  // Called when any new PhishGuard responses are received while one or more
  // WebUI tabs are open.
  void NotifyPGResponseJsListener(
      int token,
      const LoginReputationClientResponse& response);

  // Called when any new real time lookup pings are sent while one or more
  // WebUI tabs are open.
  void NotifyRTLookupPingJsListener(int token, const RTLookupRequest& request);

  // Called when any new real time lookup responses are received while one or
  // more WebUI tabs are open.
  void NotifyRTLookupResponseJsListener(int token,
                                        const RTLookupResponse& response);

  // Called when any new log messages are received while one or more WebUI tabs
  // are open.
  void NotifyLogMessageJsListener(const base::Time& timestamp,
                                  const std::string& message);

  // Callback when the CookieManager has returned the cookie.
  void OnGetCookie(const std::string& callback_id,
                   const std::vector<net::CanonicalCookie>& cookies);

  content::BrowserContext* browser_context_;

  // List that keeps all the WebUI listener objects.
  static std::vector<SafeBrowsingUIHandler*> webui_list_;

  base::WeakPtrFactory<SafeBrowsingUIHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIHandler);
};

// The WebUI for chrome://safe-browsing
class SafeBrowsingUI : public content::WebUIController {
 public:
  explicit SafeBrowsingUI(content::WebUI* web_ui);
  ~SafeBrowsingUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUI);
};

class WebUIInfoSingleton {
 public:
  static WebUIInfoSingleton* GetInstance();

  // Returns true when there is a listening chrome://safe-browsing tab.
  static bool HasListener();

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

  // Add the new message in |csbrrs_sent_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToCSBRRsSent(std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr);

  // Clear the list of the sent ClientSafeBrowsingReportRequest messages.
  void ClearCSBRRsSent();

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

  // Add the new ping to |pg_pings_| and send it to all the open
  // chrome://safe-browsing tabs. Returns a token that can be used in
  // |AddToPGReponses| to correlate a ping and response.
  int AddToPGPings(const LoginReputationClientRequest& request);

  // Add the new response to |pg_responses_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToPGResponses(int token,
                        const LoginReputationClientResponse& response);

  // Clear the list of sent PhishGuard pings and responses.
  void ClearPGPings();

  // Add the new ping to |rt_lookup_pings_|. Returns a token that can be used in
  // |AddToRTLookupResponses| to correlate a ping and response.
  int AddToRTLookupPings(const RTLookupRequest request);

  // Add the new response to |rt_lookup_responses_| and send it to all the open
  // chrome://safe-browsing tabs.
  void AddToRTLookupResponses(int token, const RTLookupResponse response);

  // Clear the list of sent RT Lookup pings and responses.
  void ClearRTLookupPings();

  // Log an arbitrary message. Frequently used for debugging.
  void LogMessage(const std::string& message);

  // Clear the log messages.
  void ClearLogMessages();

  // Notify listeners of changes to the log messages. Static to avoid this being
  // called after the destruction of the WebUIInfoSingleton
  static void NotifyLogMessageListeners(const base::Time& timestamp,
                                        const std::string& message);

  // Register the new WebUI listener object.
  void RegisterWebUIInstance(SafeBrowsingUIHandler* webui);

  // Unregister the WebUI listener object, and clean the list of reports, if
  // this is last listener.
  void UnregisterWebUIInstance(SafeBrowsingUIHandler* webui);

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

  // Get the list of the sent CSBRR reports that have been collected since the
  // oldest currently open chrome://safe-browsing tab was opened.
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>&
  csbrrs_sent() const {
    return csbrrs_sent_;
  }

  // Get the list of WebUI listener objects.
  const std::vector<SafeBrowsingUIHandler*>& webui_instances() const {
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

  // Get the list of PhishGuard pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<LoginReputationClientRequest>& pg_pings() const {
    return pg_pings_;
  }

  // Get the list of PhishGuard pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::map<int, LoginReputationClientResponse>& pg_responses() const {
    return pg_responses_;
  }

  // Get the list of real time lookup pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::vector<RTLookupRequest>& rt_lookup_pings() const {
    return rt_lookup_pings_;
  }

  // Get the list of real time lookup pings since the oldest currently open
  // chrome://safe-browsing tab was opened.
  const std::map<int, RTLookupResponse>& rt_lookup_responses() const {
    return rt_lookup_responses_;
  }

  ReferrerChainProvider* referrer_chain_provider() {
    return referrer_chain_provider_;
  }

  void set_referrer_chain_provider(ReferrerChainProvider* provider) {
    referrer_chain_provider_ = provider;
  }

  const std::vector<std::pair<base::Time, std::string>>& log_messages() {
    return log_messages_;
  }

  network::mojom::CookieManager* GetCookieManager();

  void set_network_context(SafeBrowsingNetworkContext* network_context) {
    network_context_ = network_context;
  }

  void AddListenerForTesting() { has_test_listener_ = true; }

 private:
  WebUIInfoSingleton();
  ~WebUIInfoSingleton();

  void InitializeCookieManager();

  friend struct base::DefaultSingletonTraits<WebUIInfoSingleton>;

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

  // List of CSBRRs sent since since the oldest currently open
  // chrome://safe-browsing tab was opened.
  // "ClientSafeBrowsingReportRequest" cannot be const, due to being used by
  // functions that call AllowJavascript(), which is not marked const.
  std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>> csbrrs_sent_;

  // List of PhishGuard events sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<sync_pb::UserEventSpecifics> pg_event_log_;

  // List of Security events sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<sync_pb::GaiaPasswordReuse> security_event_log_;

  // List of PhishGuard pings sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<LoginReputationClientRequest> pg_pings_;

  // List of PhishGuard responses received since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::map<int, LoginReputationClientResponse> pg_responses_;

  // List of real time lookup pings sent since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<RTLookupRequest> rt_lookup_pings_;

  // List of real time lookup responses received since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::map<int, RTLookupResponse> rt_lookup_responses_;

  // List of WebUI listener objects. "SafeBrowsingUIHandler*" cannot be const,
  // due to being used by functions that call AllowJavascript(), which is not
  // marked const.
  std::vector<SafeBrowsingUIHandler*> webui_instances_;

  // List of messages logged since the oldest currently open
  // chrome://safe-browsing tab was opened.
  std::vector<std::pair<base::Time, std::string>> log_messages_;

  // The current referrer chain provider, if any. Can be nullptr.
  ReferrerChainProvider* referrer_chain_provider_ = nullptr;

  // The current NetworkContext for Safe Browsing pings.
  SafeBrowsingNetworkContext* network_context_ = nullptr;

  // The current CookieManager for the Safe Browsing cookie.
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;

  // Whether there is a test listener.
  bool has_test_listener_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebUIInfoSingleton);
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

#endif  // COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_
