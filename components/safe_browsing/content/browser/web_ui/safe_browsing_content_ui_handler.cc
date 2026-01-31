// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_content_ui_handler.h"

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

SafeBrowsingContentUIHandler::ObserverDelegate::ObserverDelegate(
    SafeBrowsingContentUIHandler& handler)
    : handler_(handler) {}

SafeBrowsingContentUIHandler::ObserverDelegate::~ObserverDelegate() = default;

base::DictValue SafeBrowsingContentUIHandler::ObserverDelegate::
    GetFormattedTailoredVerdictOverride() {
  return handler_->GetFormattedTailoredVerdictOverride();
}

void SafeBrowsingContentUIHandler::ObserverDelegate::SendEventToHandler(
    std::string_view event_name,
    base::Value value) {
  handler_->NotifyWebUIListener(event_name, value);
}

void SafeBrowsingContentUIHandler::ObserverDelegate::SendEventToHandler(
    std::string_view event_name,
    base::ListValue& list) {
  handler_->NotifyWebUIListener(event_name, list);
}

void SafeBrowsingContentUIHandler::ObserverDelegate::SendEventToHandler(
    std::string_view event_name,
    base::DictValue dict) {
  handler_->NotifyWebUIListener(event_name, dict);
}

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
  web_ui()->RegisterMessageCallback(
      "getExperiments",
      base::BindRepeating(&SafeBrowsingUIHandler::GetExperiments,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPolicies", base::BindRepeating(&SafeBrowsingUIHandler::GetPolicies,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrefs", base::BindRepeating(&SafeBrowsingUIHandler::GetPrefs,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCookie", base::BindRepeating(&SafeBrowsingUIHandler::GetCookie,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSavedPasswords",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSavedPasswords,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDatabaseManagerInfo",
      base::BindRepeating(&SafeBrowsingUIHandler::GetDatabaseManagerInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDownloadUrlsChecked",
      base::BindRepeating(&SafeBrowsingUIHandler::GetDownloadUrlsChecked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentClientDownloadRequests",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentClientDownloadRequests,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReceivedClientDownloadResponses",
      base::BindRepeating(
          &SafeBrowsingUIHandler::GetReceivedClientDownloadResponses,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentClientPhishingRequests",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentClientPhishingRequests,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReceivedClientPhishingResponses",
      base::BindRepeating(
          &SafeBrowsingUIHandler::GetReceivedClientPhishingResponses,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentCSBRRs",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentCSBRRs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGEvents", base::BindRepeating(&SafeBrowsingUIHandler::GetPGEvents,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSecurityEvents",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSecurityEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGPings", base::BindRepeating(&SafeBrowsingUIHandler::GetPGPings,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetPGResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getURTLookupPings",
      base::BindRepeating(&SafeBrowsingUIHandler::GetURTLookupPings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getURTLookupResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetURTLookupResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHPRTLookupPings",
      base::BindRepeating(&SafeBrowsingUIHandler::GetHPRTLookupPings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHPRTLookupResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetHPRTLookupResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&SafeBrowsingUIHandler::GetLogMessages,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReferrerChain",
      base::BindRepeating(&SafeBrowsingContentUIHandler::GetReferrerChain,
                          base::Unretained(this)));
#if BUILDFLAG(IS_ANDROID)
  web_ui()->RegisterMessageCallback(
      "getReferringAppInfo",
      base::BindRepeating(&SafeBrowsingContentUIHandler::GetReferringAppInfo,
                          base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "getReportingEvents",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReportingEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDeepScans", base::BindRepeating(&SafeBrowsingUIHandler::GetDeepScans,
                                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::GetTailoredVerdictOverride,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::SetTailoredVerdictOverride,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::ClearTailoredVerdictOverride,
                          base::Unretained(this)));
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

}  // namespace safe_browsing
