// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_SAFE_BROWSING_IOS_UI_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_SAFE_BROWSING_IOS_UI_HANDLER_H_

#import "base/values.h"
#import "components/os_crypt/async/common/encryptor.h"
#import "components/safe_browsing/core/browser/download_check_result.h"
#import "components/safe_browsing/core/browser/web_ui/safe_browsing_local_state_delegate.h"
#import "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_handler.h"
#import "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_util.h"
#import "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"
#import "net/cookies/canonical_cookie.h"
#import "services/network/public/mojom/cookie_manager.mojom.h"

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace safe_browsing {

class SafeBrowsingIOSUIHandler : public web::WebUIIOSMessageHandler,
                                 public SafeBrowsingUIHandler {
 public:
  SafeBrowsingIOSUIHandler(
      web::BrowserState* browser_state,
      std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
      os_crypt_async::OSCryptAsync* os_crypt_async);

  SafeBrowsingIOSUIHandler(const SafeBrowsingIOSUIHandler&) = delete;
  SafeBrowsingIOSUIHandler& operator=(const SafeBrowsingIOSUIHandler&) = delete;

  ~SafeBrowsingIOSUIHandler() override;

  // Get the current referrer chain for a given URL.
  void GetReferrerChain(const base::ListValue& args);

  // Sets the WebUI for testing
  void SetWebUIForTesting(web::WebUIIOS* web_ui);

  // Register callbacks for WebUI messages.
  void RegisterMessages() override;

  // SafeBrowsingUIHandler::
  void RegisterMessage(std::string_view name,
                       MessageCallback callback) override;
  void ResolveCallback(const base::ValueView callback_id,
                       const base::ValueView response) override;
  PrefService* user_prefs() override;
  mojo::Remote<network::mojom::CookieManager> cookie_manager() override;
  WebUIInfoSingleton* web_ui_info_singleton() override;
  WebUIInfoSingletonEventObserver* event_observer() override;

 protected:
  // Notifies JS listeners of changes.
  void NotifyWebUIListener(std::string_view event_name,
                           const base::Value& value) override;
  void NotifyWebUIListener(std::string_view event_name,
                           const base::ListValue& list) override;
  void NotifyWebUIListener(std::string_view event_name,
                           const base::DictValue& dict) override;

 private:
  raw_ptr<web::BrowserState> browser_state_;

  // An observer object that waits for changes to the WebUIInfoSingleton and
  // updates the SafeBrowsingUIHandler.
  std::unique_ptr<WebUIInfoSingletonEventObserver> event_observer_;

  base::WeakPtrFactory<SafeBrowsingIOSUIHandler> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_SAFE_BROWSING_IOS_UI_HANDLER_H_
