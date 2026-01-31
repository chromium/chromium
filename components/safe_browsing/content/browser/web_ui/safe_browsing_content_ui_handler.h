// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_CONTENT_UI_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_CONTENT_UI_HANDLER_H_

#include "base/values.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_local_state_delegate.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_handler.h"
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

class SafeBrowsingContentUIHandler : public content::WebUIMessageHandler,
                                     public SafeBrowsingUIHandler {
 public:
  // A delegate class that communicates the changes received by the
  // WebUIInfoSingletonEventObserver to the SafeBrowsingContentUIHandler for the
  // purpose of notify JavaScript listeners.
  class ObserverDelegate : public WebUIInfoSingletonEventObserver::Delegate {
   public:
    explicit ObserverDelegate(SafeBrowsingContentUIHandler& handler);
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
    raw_ref<SafeBrowsingContentUIHandler> handler_;
  };

  SafeBrowsingContentUIHandler(
      content::BrowserContext* context,
      std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
      os_crypt_async::OSCryptAsync* os_crypt_async);

  SafeBrowsingContentUIHandler(const SafeBrowsingContentUIHandler&) = delete;
  SafeBrowsingContentUIHandler& operator=(const SafeBrowsingContentUIHandler&) =
      delete;

  ~SafeBrowsingContentUIHandler() override;

  // Callback when Javascript becomes allowed in the WebUI.
  void OnJavascriptAllowed() override;

  // Callback when Javascript becomes disallowed in the WebUI.
  void OnJavascriptDisallowed() override;

  // Get the current referrer chain for a given URL.
  void GetReferrerChain(const base::ListValue& args);

#if BUILDFLAG(IS_ANDROID)
  // Get the referring app info that launches Chroevent_observer_me on Android.
  // Always set to null if it's called from platforms other than Android.
  void GetReferringAppInfo(const base::ListValue& args);
#endif

  // Sets the WebUI for testing
  void SetWebUIForTesting(content::WebUI* web_ui);

  // Register callbacks for WebUI messages.
  void RegisterMessages() override;

  // SafeBrowsingUIHandler::
  void ResolveCallback(const base::ValueView callback_id,
                       const base::ValueView response) override;
  PrefService* user_prefs() override;
  mojo::Remote<network::mojom::CookieManager> cookie_manager() override;
  WebUIInfoSingleton* web_ui_info_singleton() override;
  WebUIInfoSingletonEventObserver* event_observer() override;

 private:
  // Notifies JS listeners of changes.
  template <typename... Values>
  void NotifyWebUIListener(std::string_view event_name,
                           const Values&... values) {
    AllowJavascript();
    FireWebUIListener(event_name, values...);
  }

  raw_ptr<content::BrowserContext> browser_context_;

  // An observer object that waits for changes to the WebUIInfoSingleton and
  // updates the SafeBrowsingUIHandler.
  std::unique_ptr<WebUIInfoSingletonEventObserver> event_observer_;

  base::WeakPtrFactory<SafeBrowsingContentUIHandler> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_CONTENT_UI_HANDLER_H_
