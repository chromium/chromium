// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_local_state_delegate.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui_util.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_info_singleton.h"
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

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/common/proto/connectors.pb.h"
#endif

namespace safe_browsing {
class SafeBrowsingUIHandler;

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
      const web_ui::ClientPhishingRequestAndToken& client_phishing_request);

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
      const web_ui::LoginReputationClientRequestAndToken& request);

  // Called when any new PhishGuard responses are received while one or more
  // WebUI tabs are open.
  void NotifyPGResponseJsListener(
      int token,
      const LoginReputationClientResponse& response);

  // Called when any new URL real time lookup pings are sent while one or more
  // WebUI tabs are open.
  void NotifyURTLookupPingJsListener(int token,
                                     const web_ui::URTLookupRequest& request);

  // Called when any new URL real time lookup responses are received while one
  // or more WebUI tabs are open.
  void NotifyURTLookupResponseJsListener(int token,
                                         const RTLookupResponse& response);

  // Called when any new hash-prefix real-time lookup pings are sent while one
  // or more WebUI tabs are open.
  void NotifyHPRTLookupPingJsListener(int token,
                                      const web_ui::HPRTLookupRequest& request);

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

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  // Called when any deep scans are updated while one or more WebUI
  // tabs are open.
  void NotifyDeepScanJsListener(const std::string& token,
                                const web_ui::DeepScanDebugData& request);
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
 protected:
  SafeBrowsingUI(content::WebUI* web_ui,
                 std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate);

  SafeBrowsingUI(const SafeBrowsingUI&) = delete;
  SafeBrowsingUI& operator=(const SafeBrowsingUI&) = delete;

  ~SafeBrowsingUI() override;
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
