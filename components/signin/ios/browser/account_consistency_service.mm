// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#include "base/bind.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Threshold (in hours) used to control whether the CHROME_CONNECTED cookie
// should be added again on a domain it was previously set.
const int kHoursThresholdToReAddCookie = 24;

// JavaScript template used to set (or delete) the CHROME_CONNECTED cookie.
// It takes 3 arguments: the domain of the cookie, its value and its expiration
// date. It also clears the legacy X-CHROME-CONNECTED cookie.
NSString* const kChromeConnectedCookieTemplate =
    @"<html><script>domain=\"%@\";"
     "document.cookie=\"X-CHROME-CONNECTED=; path=/; domain=\" + domain + \";"
     " expires=Thu, 01-Jan-1970 00:00:01 GMT\";"
     "document.cookie=\"CHROME_CONNECTED=%@; path=/; domain=\" + domain + \";"
     " expires=\" + new Date(%f).toGMTString() + \"; secure;"
     " samesite=lax;\"</script></html>";

// WebStatePolicyDecider that monitors the HTTP headers on Gaia responses,
// reacting on the X-Chrome-Manage-Accounts header and notifying its delegate.
// It also notifies the AccountConsistencyService of domains it should add the
// CHROME_CONNECTED cookie to.
class AccountConsistencyHandler : public web::WebStatePolicyDecider {
 public:
  AccountConsistencyHandler(web::WebState* web_state,
                            AccountConsistencyService* service,
                            AccountReconcilor* account_reconcilor,
                            id<ManageAccountsDelegate> delegate);

 private:
  // web::WebStatePolicyDecider override
  bool ShouldAllowResponse(NSURLResponse* response,
                           bool for_main_frame) override;
  void WebStateDestroyed() override;

  AccountConsistencyService* account_consistency_service_;  // Weak.
  AccountReconcilor* account_reconcilor_;                   // Weak.
  __weak id<ManageAccountsDelegate> delegate_;
};
}

AccountConsistencyHandler::AccountConsistencyHandler(
    web::WebState* web_state,
    AccountConsistencyService* service,
    AccountReconcilor* account_reconcilor,
    id<ManageAccountsDelegate> delegate)
    : web::WebStatePolicyDecider(web_state),
      account_consistency_service_(service),
      account_reconcilor_(account_reconcilor),
      delegate_(delegate) {}

bool AccountConsistencyHandler::ShouldAllowResponse(NSURLResponse* response,
                                                    bool for_main_frame) {
  NSHTTPURLResponse* http_response =
      base::mac::ObjCCast<NSHTTPURLResponse>(response);
  if (!http_response)
    return true;

  GURL url = net::GURLWithNSURL(http_response.URL);
  if (google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    // User is showing intent to navigate to a Google domain. Add the
    // CHROME_CONNECTED cookie to the domain if necessary.
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    account_consistency_service_->AddChromeConnectedCookieToDomain(
        domain, true /* force_update_if_too_old */);
    account_consistency_service_->AddChromeConnectedCookieToDomain(
        "google.com", true /* force_update_if_too_old */);
  }

  if (!gaia::IsGaiaSignonRealm(url.GetOrigin()))
    return true;
  NSString* manage_accounts_header = [[http_response allHeaderFields]
      objectForKey:@"X-Chrome-Manage-Accounts"];
  if (!manage_accounts_header)
    return true;

  signin::ManageAccountsParams params = signin::BuildManageAccountsParams(
      base::SysNSStringToUTF8(manage_accounts_header));

  account_reconcilor_->OnReceivedManageAccountsResponse(params.service_type);
  switch (params.service_type) {
    case signin::GAIA_SERVICE_TYPE_INCOGNITO: {
      GURL continue_url = GURL(params.continue_url);
      DLOG_IF(ERROR, !params.continue_url.empty() && !continue_url.is_valid())
          << "Invalid continuation URL: \"" << continue_url << "\"";
      [delegate_ onGoIncognito:continue_url];
      break;
    }
    case signin::GAIA_SERVICE_TYPE_SIGNUP:
    case signin::GAIA_SERVICE_TYPE_ADDSESSION:
      [delegate_ onAddAccount];
      break;
    case signin::GAIA_SERVICE_TYPE_SIGNOUT:
    case signin::GAIA_SERVICE_TYPE_DEFAULT:
      [delegate_ onManageAccounts];
      break;
    case signin::GAIA_SERVICE_TYPE_NONE:
      NOTREACHED();
      break;
  }

  // WKWebView loads a blank page even if the response code is 204
  // ("No Content"). http://crbug.com/368717
  //
  // Manage accounts responses are handled via native UI. Abort this request
  // for the following reasons:
  // * Avoid loading a blank page in WKWebView.
  // * Avoid adding this request to history.
  return false;
}

void AccountConsistencyHandler::WebStateDestroyed() {
  account_consistency_service_->RemoveWebStateHandler(web_state());
}

// WKWebView navigation delegate that calls its callback every time a navigation
// has finished.
@interface AccountConsistencyNavigationDelegate : NSObject<WKNavigationDelegate>

// Designated initializer. |callback| will be called every time a navigation has
// finished. |callback| must not be empty.
- (instancetype)initWithCallback:(const base::Closure&)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end

@implementation AccountConsistencyNavigationDelegate {
  // Callback that will be called every time a navigation has finished.
  base::Closure _callback;
}

- (instancetype)initWithCallback:(const base::Closure&)callback {
  self = [super init];
  if (self) {
    DCHECK(!callback.is_null());
    _callback = callback;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  _callback.Run();
}

@end

const char AccountConsistencyService::kDomainsWithCookiePref[] =
    "signin.domains_with_cookie";

AccountConsistencyService::CookieRequest
AccountConsistencyService::CookieRequest::CreateAddCookieRequest(
    const std::string& domain) {
  AccountConsistencyService::CookieRequest cookie_request;
  cookie_request.request_type = ADD_CHROME_CONNECTED_COOKIE;
  cookie_request.domain = domain;
  return cookie_request;
}

AccountConsistencyService::CookieRequest
AccountConsistencyService::CookieRequest::CreateRemoveCookieRequest(
    const std::string& domain,
    base::OnceClosure callback) {
  AccountConsistencyService::CookieRequest cookie_request;
  cookie_request.request_type = REMOVE_CHROME_CONNECTED_COOKIE;
  cookie_request.domain = domain;
  cookie_request.callback = std::move(callback);
  return cookie_request;
}

AccountConsistencyService::CookieRequest::CookieRequest() = default;

AccountConsistencyService::CookieRequest::~CookieRequest() = default;

AccountConsistencyService::CookieRequest::CookieRequest(
    AccountConsistencyService::CookieRequest&&) = default;

AccountConsistencyService::AccountConsistencyService(
    web::BrowserState* browser_state,
    PrefService* prefs,
    AccountReconcilor* account_reconcilor,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    signin::IdentityManager* identity_manager)
    : browser_state_(browser_state),
      prefs_(prefs),
      account_reconcilor_(account_reconcilor),
      cookie_settings_(cookie_settings),
      identity_manager_(identity_manager),
      applying_cookie_requests_(false) {
  identity_manager_->AddObserver(this);
  ActiveStateManager::FromBrowserState(browser_state_)->AddObserver(this);
  LoadFromPrefs();
  if (identity_manager_->HasPrimaryAccount()) {
    AddChromeConnectedCookies();
  } else {
    RemoveChromeConnectedCookies(base::OnceClosure());
  }
}

AccountConsistencyService::~AccountConsistencyService() {
  DCHECK(!web_view_);
  DCHECK(!navigation_delegate_);
}

// static
void AccountConsistencyService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      AccountConsistencyService::kDomainsWithCookiePref);
}

void AccountConsistencyService::SetWebStateHandler(
    web::WebState* web_state,
    id<ManageAccountsDelegate> delegate) {
  DCHECK_EQ(0u, web_state_handlers_.count(web_state));
  web_state_handlers_[web_state].reset(new AccountConsistencyHandler(
      web_state, this, account_reconcilor_, delegate));
}

void AccountConsistencyService::RemoveWebStateHandler(
    web::WebState* web_state) {
  DCHECK_LT(0u, web_state_handlers_.count(web_state));
  web_state_handlers_.erase(web_state);
}

bool AccountConsistencyService::ShouldAddChromeConnectedCookieToDomain(
    const std::string& domain,
    bool force_update_if_too_old) {
  auto it = last_cookie_update_map_.find(domain);
  if (it == last_cookie_update_map_.end()) {
    // |domain| isn't in the map, always add the cookie.
    return true;
  }
  if (!force_update_if_too_old) {
    // |domain| is in the map and the cookie is considered valid. Don't add it
    // again.
    return false;
  }
  return (base::Time::Now() - it->second) >
         base::TimeDelta::FromHours(kHoursThresholdToReAddCookie);
}

void AccountConsistencyService::RemoveChromeConnectedCookies(
    base::OnceClosure callback) {
  DCHECK(!browser_state_->IsOffTheRecord());
  if (last_cookie_update_map_.empty()) {
    if (!callback.is_null())
      std::move(callback).Run();
    return;
  }
  std::map<std::string, base::Time> last_cookie_update_map =
      last_cookie_update_map_;
  auto iter_last_item = std::prev(last_cookie_update_map.end());
  for (auto iter = last_cookie_update_map.begin(); iter != iter_last_item;
       iter++) {
    RemoveChromeConnectedCookieFromDomain(iter->first, base::OnceClosure());
  }
  RemoveChromeConnectedCookieFromDomain(iter_last_item->first,
                                        std::move(callback));
}

void AccountConsistencyService::AddChromeConnectedCookieToDomain(
    const std::string& domain,
    bool force_update_if_too_old) {
  if (!ShouldAddChromeConnectedCookieToDomain(domain,
                                              force_update_if_too_old)) {
    return;
  }
  last_cookie_update_map_[domain] = base::Time::Now();
  cookie_requests_.push_back(CookieRequest::CreateAddCookieRequest(domain));
  ApplyCookieRequests();
}

void AccountConsistencyService::RemoveChromeConnectedCookieFromDomain(
    const std::string& domain,
    base::OnceClosure callback) {
  DCHECK_NE(0ul, last_cookie_update_map_.count(domain));
  last_cookie_update_map_.erase(domain);
  cookie_requests_.push_back(
      CookieRequest::CreateRemoveCookieRequest(domain, std::move(callback)));
  ApplyCookieRequests();
}

void AccountConsistencyService::LoadFromPrefs() {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(kDomainsWithCookiePref);
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    last_cookie_update_map_[it.key()] = base::Time();
  }
}

void AccountConsistencyService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  ActiveStateManager::FromBrowserState(browser_state_)->RemoveObserver(this);
  ResetWKWebView();
  web_state_handlers_.clear();
}

void AccountConsistencyService::ApplyCookieRequests() {
  if (applying_cookie_requests_) {
    // A cookie request is already being applied, the following ones will be
    // handled as soon as the current one is done.
    return;
  }
  if (cookie_requests_.empty()) {
    return;
  }
  if (!ActiveStateManager::FromBrowserState(browser_state_)->IsActive()) {
    // Web view usage isn't active for now, ignore cookie requests for now and
    // wait to be notified that it became active again.
    return;
  }
  applying_cookie_requests_ = true;

  const GURL url("https://" + cookie_requests_.front().domain);
  std::string cookie_value = "";
  // Expiration date of the cookie in the JavaScript convention of time, a
  // number of milliseconds since the epoch.
  double expiration_date = 0;
  switch (cookie_requests_.front().request_type) {
    case ADD_CHROME_CONNECTED_COOKIE:
      cookie_value = signin::BuildMirrorRequestCookieIfPossible(
          url, identity_manager_->GetPrimaryAccountInfo().gaia,
          signin::AccountConsistencyMethod::kMirror, cookie_settings_.get(),
          signin::PROFILE_MODE_DEFAULT);
      if (cookie_value.empty()) {
        // Don't add the cookie. Tentatively correct |last_cookie_update_map_|.
        last_cookie_update_map_.erase(cookie_requests_.front().domain);
        FinishedApplyingCookieRequest(false);
        return;
      }
      // Create expiration date of Now+2y to roughly follow the SAPISID cookie.
      expiration_date =
          (base::Time::Now() + base::TimeDelta::FromDays(730)).ToJsTime();
      break;
    case REMOVE_CHROME_CONNECTED_COOKIE:
      // Nothing to do. Default values correspond to removing the cookie (no
      // value, expiration date in the past).
      break;
  }
  NSString* html = [NSString
      stringWithFormat:kChromeConnectedCookieTemplate,
                       base::SysUTF8ToNSString(url.host()),
                       base::SysUTF8ToNSString(cookie_value), expiration_date];
  // Load an HTML string with embedded JavaScript that will set or remove the
  // cookie. By setting the base URL to |url|, this effectively allows to modify
  // cookies on the correct domain without having to do a network request.
  [GetWKWebView() loadHTMLString:html baseURL:net::NSURLWithGURL(url)];
}

void AccountConsistencyService::FinishedApplyingCookieRequest(bool success) {
  DCHECK(!cookie_requests_.empty());
  CookieRequest& request = cookie_requests_.front();
  if (success) {
    DictionaryPrefUpdate update(
        prefs_, AccountConsistencyService::kDomainsWithCookiePref);
    switch (request.request_type) {
      case ADD_CHROME_CONNECTED_COOKIE:
        // Add request.domain to prefs, use |true| as a dummy value (that is
        // never used), as the dictionary is used as a set.
        update->SetKey(request.domain, base::Value(true));
        break;
      case REMOVE_CHROME_CONNECTED_COOKIE:
        // Remove request.domain from prefs.
        update->RemoveWithoutPathExpansion(request.domain, nullptr);
        break;
    }
  }
  base::OnceClosure callback(std::move(request.callback));
  cookie_requests_.pop_front();
  applying_cookie_requests_ = false;
  ApplyCookieRequests();
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

WKWebView* AccountConsistencyService::GetWKWebView() {
  if (!ActiveStateManager::FromBrowserState(browser_state_)->IsActive()) {
    // |browser_state_| is not active, WKWebView linked to this browser state
    // should not exist or be created.
    return nil;
  }
  if (!web_view_) {
    web_view_ = BuildWKWebView();
    navigation_delegate_ = [[AccountConsistencyNavigationDelegate alloc]
        initWithCallback:base::Bind(&AccountConsistencyService::
                                        FinishedApplyingCookieRequest,
                                    base::Unretained(this), true)];
    [web_view_ setNavigationDelegate:navigation_delegate_];
  }
  return web_view_;
}

WKWebView* AccountConsistencyService::BuildWKWebView() {
  return web::BuildWKWebView(CGRectZero, browser_state_);
}

void AccountConsistencyService::ResetWKWebView() {
  [web_view_ setNavigationDelegate:nil];
  [web_view_ stopLoading];
  web_view_ = nil;
  navigation_delegate_ = nil;
  applying_cookie_requests_ = false;
}

void AccountConsistencyService::AddChromeConnectedCookies() {
  DCHECK(!browser_state_->IsOffTheRecord());
  // These cookie request are preventive and not a strong signal (unlike
  // navigation to a domain). Don't force update the old cookies in this case.
  AddChromeConnectedCookieToDomain("google.com",
                                   false /* force_update_if_too_old */);
  AddChromeConnectedCookieToDomain("youtube.com",
                                   false /* force_update_if_too_old */);
}

void AccountConsistencyService::OnBrowsingDataRemoved() {
  // CHROME_CONNECTED cookies have been removed, update internal state
  // accordingly.
  ResetWKWebView();
  for (auto& cookie_request : cookie_requests_) {
    base::OnceClosure callback(std::move(cookie_request.callback));
    if (!callback.is_null()) {
      std::move(callback).Run();
    }
  }
  cookie_requests_.clear();
  last_cookie_update_map_.clear();
  base::DictionaryValue dict;
  prefs_->Set(kDomainsWithCookiePref, dict);

  // SAPISID cookie has been removed, notify the GCMS.
  // TODO(https://crbug.com/930582) : Remove the need to expose this method
  // or move it to the network::CookieManager.
  identity_manager_->GetAccountsCookieMutator()->ForceTriggerOnCookieChange();
}

void AccountConsistencyService::OnPrimaryAccountSet(
    const CoreAccountInfo& account_info) {
  AddChromeConnectedCookies();
}

void AccountConsistencyService::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_account_info) {
  // There is not need to remove CHROME_CONNECTED cookies on |GoogleSignedOut|
  // events as these cookies will be removed by the GaiaCookieManagerServer
  // right before fetching the Gaia logout request.
}

void AccountConsistencyService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  AddChromeConnectedCookies();
}

void AccountConsistencyService::OnActive() {
  // |browser_state_| is now active. There might be some pending cookie requests
  // to apply.
  ApplyCookieRequests();
}

void AccountConsistencyService::OnInactive() {
  // |browser_state_| is now inactive. Stop using |web_view_| and don't create
  // a new one until it is active.
  ResetWKWebView();
}
