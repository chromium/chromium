// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/web_ui/safe_browsing_ios_ui_handler.h"

#import "components/os_crypt/async/browser/os_crypt_async.h"
#import "components/os_crypt/async/common/encryptor.h"
#import "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_handler.h"
#import "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer_impl.h"
#import "components/safe_browsing/core/common/proto/csd.to_value.h"
#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"
#import "components/user_prefs/user_prefs.h"

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#import "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#endif

namespace safe_browsing {

SafeBrowsingIOSUIHandler::SafeBrowsingIOSUIHandler(
    web::BrowserState* browser_state,
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : SafeBrowsingUIHandler(std::move(delegate), os_crypt_async),
      browser_state_(browser_state) {
  auto observer_delegate = std::make_unique<ObserverDelegate>(*this);
  event_observer_ = std::make_unique<WebUIInfoSingletonEventObserverImpl>(
      std::move(observer_delegate));
}

SafeBrowsingIOSUIHandler::~SafeBrowsingIOSUIHandler() {
  WebUIIOSInfoSingleton::GetInstance()->UnregisterWebUIInstance(
      event_observer_.get());
}

void SafeBrowsingIOSUIHandler::GetReferrerChain(const base::ListValue& args) {
  DCHECK_GE(args.size(), 2U);
  const std::string& callback_id = args[0].GetString();

  // Referrer chain provider is currently not available on iOS. Once it
  // is implemented, inject it to enable referrer chain in real time
  // requests.
  safe_browsing::ReferrerChainProvider* referrer_chain_provider = nullptr;

  if (!referrer_chain_provider) {
    ResolveCallback(callback_id, "");
    return;
  }

  // Once Referrer chain provider is implemented for iOS, bring this function
  // to parity with SafeBrowsingContentUIHandler::GetReferrerChain.
}

void SafeBrowsingIOSUIHandler::SetWebUIForTesting(web::WebUIIOS* web_ui) {
  set_web_ui(web_ui);
}

void SafeBrowsingIOSUIHandler::RegisterMessages() {
  SafeBrowsingUIHandler::RegisterMessages();
  RegisterMessage(
      "getReferrerChain",
      base::BindRepeating(&SafeBrowsingIOSUIHandler::GetReferrerChain,
                          base::Unretained(this)));
}

void SafeBrowsingIOSUIHandler::RegisterMessage(std::string_view name,
                                               MessageCallback callback) {
  web_ui()->RegisterMessageCallback(name, std::move(callback));
}

void SafeBrowsingIOSUIHandler::ResolveCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  web_ui()->ResolveJavascriptCallback(callback_id, response);
}

PrefService* SafeBrowsingIOSUIHandler::user_prefs() {
  return user_prefs::UserPrefs::Get(browser_state_);
}

mojo::Remote<network::mojom::CookieManager>
SafeBrowsingIOSUIHandler::cookie_manager() {
  return WebUIIOSInfoSingleton::GetInstance()->GetCookieManager();
}

WebUIInfoSingleton* SafeBrowsingIOSUIHandler::web_ui_info_singleton() {
  return WebUIIOSInfoSingleton::GetInstance();
}

WebUIInfoSingletonEventObserver* SafeBrowsingIOSUIHandler::event_observer() {
  return event_observer_.get();
}

void SafeBrowsingIOSUIHandler::NotifyWebUIListener(std::string_view event_name,
                                                   const base::Value& value) {
  web_ui()->FireWebUIListener(event_name, value);
}

void SafeBrowsingIOSUIHandler::NotifyWebUIListener(
    std::string_view event_name,
    const base::ListValue& list) {
  web_ui()->FireWebUIListener(event_name, list);
}

void SafeBrowsingIOSUIHandler::NotifyWebUIListener(
    std::string_view event_name,
    const base::DictValue& dict) {
  web_ui()->FireWebUIListener(event_name, dict);
}

}  // namespace safe_browsing
