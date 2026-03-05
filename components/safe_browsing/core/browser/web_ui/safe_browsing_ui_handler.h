// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_HANDLER_H_

#include "base/values.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_local_state_delegate.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_util.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace os_crypt_async {
class Encryptor;
class OSCryptAsync;
}  // namespace os_crypt_async

namespace safe_browsing {
class WebUIInfoSingleton;
class WebUIInfoSingletonEventObserver;

using MessageCallback = base::RepeatingCallback<void(const base::ListValue&)>;

class SafeBrowsingUIHandler {
 public:
  // A delegate class that communicates the changes received by the
  // WebUIInfoSingletonEventObserver to the SafeBrowsingUIHandler for the
  // purpose of notify JavaScript listeners.
  class ObserverDelegate : public WebUIInfoSingletonEventObserver::Delegate {
   public:
    explicit ObserverDelegate(SafeBrowsingUIHandler& handler);
    ~ObserverDelegate() override;

    // WebUIInfoSingletonEventObserver::Delegate::
    base::DictValue GetFormattedTailoredVerdictOverride() override;
    void SendEventToHandler(std::string_view event_name,
                            base::Value value) override;
    void SendEventToHandler(std::string_view event_name,
                            base::ListValue& list) override;
    void SendEventToHandler(std::string_view event_name,
                            base::DictValue dict) override;

   private:
    raw_ref<SafeBrowsingUIHandler> handler_;
  };

  SafeBrowsingUIHandler(
      std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
      os_crypt_async::OSCryptAsync* os_crypt_async);
  SafeBrowsingUIHandler(const SafeBrowsingUIHandler&) = delete;
  SafeBrowsingUIHandler& operator=(const SafeBrowsingUIHandler&) = delete;
  ~SafeBrowsingUIHandler();

  // Get the experiments that are currently enabled per Chrome instance.
  void GetExperiments(const base::ListValue& args);

  // Get the Safe Browsing related preferences for the current user.
  void GetPrefs(const base::ListValue& args);

  // Get the Safe Browsing related policies for the current user.
  void GetPolicies(const base::ListValue& args);

  // Get the Safe Browsing cookie.
  void GetCookie(const base::ListValue& args);

  // Get the current captured passwords.
  void GetSavedPasswords(const base::ListValue& args);

  // Get the information related to the Safe Browsing database and full hash
  // cache.
  void GetDatabaseManagerInfo(const base::ListValue& args);

  // Get the download URLs that have been checked since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetDownloadUrlsChecked(const base::ListValue& args);

  // Get the ClientDownloadRequests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSentClientDownloadRequests(const base::ListValue& args);

  // Get the ClientDownloadResponses that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetReceivedClientDownloadResponses(const base::ListValue& args);

  // Get the ClientPhishingRequests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSentClientPhishingRequests(const base::ListValue& args);

  // Get the ClientPhishingResponses that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetReceivedClientPhishingResponses(const base::ListValue& args);

  // Get the ThreatDetails that have been collected since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetSentCSBRRs(const base::ListValue& args);

  // Get the PhishGuard events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetPGEvents(const base::ListValue& args);

  // Get the Security events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetSecurityEvents(const base::ListValue& args);

  // Get the PhishGuard pings that have been sent since the oldest currently
  // open chrome://safe-browsing tab was opened.
  void GetPGPings(const base::ListValue& args);

  // Get the PhishGuard responses that have been received since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetPGResponses(const base::ListValue& args);

  // Get the URL real time lookup pings that have been sent since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetURTLookupPings(const base::ListValue& args);

  // Get the URL real time lookup responses that have been received since the
  // oldest currently open chrome://safe-browsing tab was opened.
  void GetURTLookupResponses(const base::ListValue& args);

  // Get the hash-prefix real-time lookup pings that have been sent since the
  // oldest currently open chrome://safe-browsing tab was opened.
  void GetHPRTLookupPings(const base::ListValue& args);

  // Get the hash-prefix real-time lookup responses that have been received
  // since the oldest currently open chrome://safe-browsing tab was opened.
  void GetHPRTLookupResponses(const base::ListValue& args);

  // Get the list of log messages that have been received since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetLogMessages(const base::ListValue& args);

  // Get the reporting events that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetReportingEvents(const base::ListValue& args);

  // Get the deep scanning requests that have been collected since the oldest
  // currently open chrome://safe-browsing tab was opened.
  void GetDeepScans(const base::ListValue& args);

  // Get the most recently set tailored verdict override, if its setting
  // chrome://safe-browsing tab has not been closed.
  void GetTailoredVerdictOverride(const base::ListValue& args);

  // Sets the tailored verdict override from args.
  void SetTailoredVerdictOverride(const base::ListValue& args);

  // Clears the current tailored verdict override.
  void ClearTailoredVerdictOverride(const base::ListValue& args);

  // Registers all the messages from the DOM to which the handler class intends
  // to listen.
  void RegisterMessages();

  // Registers the message and callback with the WebUI.
  virtual void RegisterMessage(std::string_view name,
                               MessageCallback callback) = 0;

  // Resolves Javascript requests initiated with returned promises.
  virtual void ResolveCallback(const base::ValueView callback_id,
                               const base::ValueView response) = 0;

  // Accessor method that returns the observer object.
  virtual WebUIInfoSingletonEventObserver* event_observer() = 0;

  // Accessor method that returns the user PrefService object.
  virtual PrefService* user_prefs() = 0;

  // Accessor method that returns the CookieManager object.
  virtual mojo::Remote<network::mojom::CookieManager> cookie_manager() = 0;

  // Accessor method that returns the WebUIInfoSingleton object.
  virtual WebUIInfoSingleton* web_ui_info_singleton() = 0;

 protected:
  // Gets the tailored verdict override in a format for displaying.
  base::DictValue GetFormattedTailoredVerdictOverride();

  // Notifies JS listeners of changes.
  virtual void NotifyWebUIListener(std::string_view event_name,
                                   const base::Value& value) = 0;
  virtual void NotifyWebUIListener(std::string_view event_name,
                                   const base::ListValue& list) = 0;
  virtual void NotifyWebUIListener(std::string_view event_name,
                                   const base::DictValue& dict) = 0;

 private:
  // Sends formatted tailored verdict override information to the WebUI.
  void ResolveTailoredVerdictOverrideCallback(const std::string& callback_id);

  // Callback when the CookieManager has returned the cookie.
  void OnGetCookie(const std::string& callback_id,
                   const std::vector<net::CanonicalCookie>& cookies);

  void GetSavedPasswordsImpl(const std::string& callback_id,
                             os_crypt_async::Encryptor encryptor);

  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote_;

  // Returns PrefService for local state.
  std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate_;

  const raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;

  base::WeakPtrFactory<SafeBrowsingUIHandler> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_HANDLER_H_
