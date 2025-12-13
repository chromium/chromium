// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_HANDLER_H_

#include "base/values.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_local_state_delegate.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_util.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace os_crypt_async {
class OSCryptAsync;
}

namespace safe_browsing {

class SafeBrowsingUIHandler : public content::WebUIMessageHandler {
 public:
  // A delegate class that communicates the changes received by the
  // WebUIInfoSingletonEventObserver to the SafeBrowsingUIHandler for the
  // purpose of notify JavaScript listeners.
  class ObserverDelegate : public WebUIInfoSingletonEventObserver::Delegate {
   public:
    explicit ObserverDelegate(SafeBrowsingUIHandler& handler);
    ~ObserverDelegate() override;

    // WebUIInfoSingletonEventObserver::Delegate::
    base::Value::Dict GetFormattedTailoredVerdictOverride() override;
    void SendEventToHandler(std::string_view event_name,
                            base::Value value) override;
    void SendEventToHandler(std::string_view event_name,
                            base::Value::List& list) override;
    void SendEventToHandler(std::string_view event_name,
                            base::Value::Dict dict) override;

   private:
    raw_ref<SafeBrowsingUIHandler> handler_;
  };

  SafeBrowsingUIHandler(
      content::BrowserContext* context,
      std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
      os_crypt_async::OSCryptAsync* os_crypt_async);

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

  // Get the ClientDownloadResponses that have been collected since the oldest
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

  // Accessor method that returns the observer object.
  WebUIInfoSingletonEventObserver* event_observer();

  // Sets the WebUI for testing
  void SetWebUIForTesting(content::WebUI* web_ui);

 private:
  // Notifies JS listeners of changes.
  template <typename... Values>
  void NotifyWebUIListener(std::string_view event_name,
                           const Values&... values) {
    AllowJavascript();
    FireWebUIListener(event_name, values...);
  }

  // Gets the tailored verdict override in a format for displaying.
  base::Value::Dict GetFormattedTailoredVerdictOverride();

  // Sends formatted tailored verdict override information to the WebUI.
  void ResolveTailoredVerdictOverrideCallback(const std::string& callback_id);

  // Callback when the CookieManager has returned the cookie.
  void OnGetCookie(const std::string& callback_id,
                   const std::vector<net::CanonicalCookie>& cookies);

  void GetSavedPasswordsImpl(const std::string& callback_id,
                             os_crypt_async::Encryptor encryptor);

  raw_ptr<content::BrowserContext> browser_context_;

  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;

  // List that keeps all the WebUI listener objects.
  static std::vector<SafeBrowsingUIHandler*> webui_list_;

  // Returns PrefService for local state.
  std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate_;

  const raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;

  // An observer object that waits for changes to the WebUIInfoSingleton and
  // updates the SafeBrowsingUIHandler.
  std::unique_ptr<WebUIInfoSingletonEventObserver> event_observer_;

  base::WeakPtrFactory<SafeBrowsingUIHandler> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_HANDLER_H_
