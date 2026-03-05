// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_content_ui_handler.h"

#include "base/values.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer_impl.h"
#include "components/safe_browsing/core/common/proto/csd.to_value.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/global_routing_id.h"

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#endif

namespace safe_browsing {

SafeBrowsingContentUIHandler::SafeBrowsingContentUIHandler(
    content::BrowserContext* context,
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : SafeBrowsingUIHandler(std::move(delegate), os_crypt_async),
      browser_context_(context) {
  auto observer_delegate = std::make_unique<ObserverDelegate>(*this);
  event_observer_ = std::make_unique<WebUIInfoSingletonEventObserverImpl>(
      std::move(observer_delegate));
}

SafeBrowsingContentUIHandler::~SafeBrowsingContentUIHandler() {
  WebUIContentInfoSingleton::GetInstance()->UnregisterWebUIInstance(
      event_observer_.get());
}

void SafeBrowsingContentUIHandler::OnJavascriptAllowed() {
  // We don't want to register the SafeBrowsingContentUIHandler with the
  // WebUIInfoSingleton at construction, since this can lead to
  // messages being sent to the renderer before it's ready. So register it here.
  WebUIContentInfoSingleton::GetInstance()->RegisterWebUIInstance(
      event_observer_.get());
}

void SafeBrowsingContentUIHandler::OnJavascriptDisallowed() {
  // In certain situations, Javascript can become disallowed before the
  // destructor is called (e.g. tab refresh/renderer crash). In these situation,
  // we want to stop receiving JS messages.
  WebUIContentInfoSingleton::GetInstance()->UnregisterWebUIInstance(
      event_observer_.get());
}

void SafeBrowsingContentUIHandler::GetReferrerChain(
    const base::ListValue& args) {
  DCHECK_GE(args.size(), 2U);
  const std::string& url_string = args[1].GetString();

  ReferrerChainProvider* provider =
      WebUIContentInfoSingleton::GetInstance()->GetReferrerChainProvider(
          browser_context_);

  const std::string& callback_id = args[0].GetString();

  if (!provider) {
    ResolveCallback(callback_id, "");
    return;
  }

  ReferrerChain referrer_chain;
  provider->IdentifyReferrerChainByEventURL(
      GURL(url_string), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 2, &referrer_chain);

  base::ListValue referrer_list;
  for (const ReferrerChainEntry& entry : referrer_chain) {
    referrer_list.Append(ToValue(entry));
  }

  std::string referrer_chain_serialized = web_ui::SerializeJson(referrer_list);

  ResolveCallback(callback_id, referrer_chain_serialized);
}

#if BUILDFLAG(IS_ANDROID)
void SafeBrowsingContentUIHandler::GetReferringAppInfo(
    const base::ListValue& args) {
  base::DictValue referring_app_value;
  internal::ReferringAppInfo info =
      WebUIContentInfoSingleton::GetInstance()->GetReferringAppInfo(
          web_ui()->GetWebContents());
  referring_app_value = web_ui::SerializeReferringAppInfo(info);

  std::string referring_app_serialized =
      web_ui::SerializeJson(referring_app_value);

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, referring_app_serialized);
}
#endif

void SafeBrowsingContentUIHandler::SetWebUIForTesting(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void SafeBrowsingContentUIHandler::RegisterMessages() {
  SafeBrowsingUIHandler::RegisterMessages();
  RegisterMessage(
      "getReferrerChain",
      base::BindRepeating(&SafeBrowsingContentUIHandler::GetReferrerChain,
                          base::Unretained(this)));
#if BUILDFLAG(IS_ANDROID)
  RegisterMessage(
      "getReferringAppInfo",
      base::BindRepeating(&SafeBrowsingContentUIHandler::GetReferringAppInfo,
                          base::Unretained(this)));
#endif
}

void SafeBrowsingContentUIHandler::RegisterMessage(std::string_view name,
                                                   MessageCallback callback) {
  web_ui()->RegisterMessageCallback(name, std::move(callback));
}

void SafeBrowsingContentUIHandler::ResolveCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  AllowJavascript();
  ResolveJavascriptCallback(callback_id, response);
}

PrefService* SafeBrowsingContentUIHandler::user_prefs() {
  return user_prefs::UserPrefs::Get(browser_context_);
}

mojo::Remote<network::mojom::CookieManager>
SafeBrowsingContentUIHandler::cookie_manager() {
  return WebUIContentInfoSingleton::GetInstance()->GetCookieManager(
      browser_context_);
}

WebUIInfoSingleton* SafeBrowsingContentUIHandler::web_ui_info_singleton() {
  return WebUIContentInfoSingleton::GetInstance();
}

WebUIInfoSingletonEventObserver*
SafeBrowsingContentUIHandler::event_observer() {
  return event_observer_.get();
}

void SafeBrowsingContentUIHandler::NotifyWebUIListener(
    std::string_view event_name,
    const base::Value& value) {
  AllowJavascript();
  FireWebUIListener(event_name, value);
}
void SafeBrowsingContentUIHandler::NotifyWebUIListener(
    std::string_view event_name,
    const base::ListValue& list) {
  AllowJavascript();
  FireWebUIListener(event_name, list);
}
void SafeBrowsingContentUIHandler::NotifyWebUIListener(
    std::string_view event_name,
    const base::DictValue& dict) {
  AllowJavascript();
  FireWebUIListener(event_name, dict);
}

}  // namespace safe_browsing
