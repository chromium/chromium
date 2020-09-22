// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#include "base/bind.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
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
#include "google_apis/gaia/gaia_urls.h"
#include "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The validity of CHROME_CONNECTED cookies is one day maximum as a
// precaution to ensure that the cookie is regenerated in the case that it
// is removed or invalidated.
constexpr base::TimeDelta kDelayThresholdToUpdateChromeConnectedCookie =
    base::TimeDelta::FromHours(24);
// The validity of the Gaia cookie on the Google domain is one hour to
// ensure that Mirror account consistency is respected in light of the more
// restrictive Intelligent Tracking Prevention (ITP) guidelines in iOS 14
// that may remove or invalidate Gaia cookies on the Google domain.
constexpr base::TimeDelta kDelayThresholdToUpdateGaiaCookie =
    base::TimeDelta::FromHours(1);

const char* kGoogleDomain = "google.com";
const char* kYoutubeDomain = "youtube.com";

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
  // web::WebStatePolicyDecider override.
  // Decides on navigation corresponding to |response| whether the navigation
  // should continue and updates authentication cookies on Google domains.
  void ShouldAllowResponse(
      NSURLResponse* response,
      bool for_main_frame,
      base::OnceCallback<void(PolicyDecision)> callback) override;
  void WebStateDestroyed() override;

  AccountConsistencyService* account_consistency_service_;  // Weak.
  AccountReconcilor* account_reconcilor_;                   // Weak.
  __weak id<ManageAccountsDelegate> delegate_;
};
}  // namespace

const base::Feature kRestoreGAIACookiesIfDeleted{
    "RestoreGAIACookiesIfDeleted", base::FEATURE_DISABLED_BY_DEFAULT};

AccountConsistencyHandler::AccountConsistencyHandler(
    web::WebState* web_state,
    AccountConsistencyService* service,
    AccountReconcilor* account_reconcilor,
    id<ManageAccountsDelegate> delegate)
    : web::WebStatePolicyDecider(web_state),
      account_consistency_service_(service),
      account_reconcilor_(account_reconcilor),
      delegate_(delegate) {}

void AccountConsistencyHandler::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    base::OnceCallback<void(PolicyDecision)> callback) {
  NSHTTPURLResponse* http_response =
      base::mac::ObjCCast<NSHTTPURLResponse>(response);
  if (!http_response) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  GURL url = net::GURLWithNSURL(http_response.URL);
  // Logged-in user is showing intent to navigate to a Google domain where we
  // will need to set a CHROME_CONNECTED cookie if it is not already set.
  if (signin::IsUrlEligibleForMirrorCookie(url)) {
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    account_consistency_service_->SetChromeConnectedCookieWithDomain(domain);
    account_consistency_service_->SetChromeConnectedCookieWithDomain(
        kGoogleDomain);
    account_consistency_service_->SetGaiaCookiesIfDeleted();
  }

  if (!gaia::IsGaiaSignonRealm(url.GetOrigin())) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }
  NSString* manage_accounts_header = [[http_response allHeaderFields]
      objectForKey:@"X-Chrome-Manage-Accounts"];
  if (!manage_accounts_header) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

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
  std::move(callback).Run(PolicyDecision::Cancel());
}

void AccountConsistencyHandler::WebStateDestroyed() {
  account_consistency_service_->RemoveWebStateHandler(web_state());
}

const char AccountConsistencyService::kChromeConnectedCookieName[] =
    "CHROME_CONNECTED";

const char AccountConsistencyService::kGaiaCookieName[] = "SAPISID";

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
  LoadFromPrefs();
  if (identity_manager_->HasPrimaryAccount()) {
    AddChromeConnectedCookies();
  } else {
    RemoveChromeConnectedCookies(base::OnceClosure());
  }
}

AccountConsistencyService::~AccountConsistencyService() {
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

void AccountConsistencyService::SetGaiaCookiesIfDeleted() {
  // We currently enforce a time threshold to update the Gaia cookie
  // to prevent calling the expensive call to the cookie manager's
  // |GetAllCookies|.
  if (base::Time::Now() - last_gaia_cookie_verification_time_ <
      kDelayThresholdToUpdateGaiaCookie) {
    return;
  }
  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  cookie_manager->GetCookieList(
      GaiaUrls::GetInstance()->secure_google_url(),
      net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(
          &AccountConsistencyService::TriggerGaiaCookieChangeIfDeleted,
          base::Unretained(this)));
  last_gaia_cookie_verification_time_ = base::Time::Now();
}

void AccountConsistencyService::TriggerGaiaCookieChangeIfDeleted(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& unused_excluded_cookies) {
  for (const auto& cookie : cookie_list) {
    if (cookie.cookie.Name() == kGaiaCookieName) {
      LogIOSGaiaCookiesPresentOnNavigation(true);
      return;
    }
  }

  // The SAPISID cookie may have been deleted previous to this update due to
  // ITP restrictions marking Google domains as potential trackers.
  LogIOSGaiaCookiesPresentOnNavigation(false);

  if (!base::FeatureList::IsEnabled(kRestoreGAIACookiesIfDeleted)) {
    return;
  }
  // Re-generate cookie to ensure that the user is properly signed in.
  identity_manager_->GetAccountsCookieMutator()->ForceTriggerOnCookieChange();
}

void AccountConsistencyService::LogIOSGaiaCookiesPresentOnNavigation(
    bool is_present) {
  base::UmaHistogramBoolean("Signin.IOSGaiaCookiePresentOnNavigation",
                            is_present);
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

void AccountConsistencyService::SetChromeConnectedCookieWithDomain(
    const std::string& domain) {
  SetChromeConnectedCookieWithDomain(
      domain, kDelayThresholdToUpdateChromeConnectedCookie);
}

void AccountConsistencyService::SetChromeConnectedCookieWithDomain(
    const std::string& domain,
    const base::TimeDelta& cookie_refresh_interval) {
  if (!ShouldSetChromeConnectedCookieToDomain(domain,
                                              cookie_refresh_interval)) {
    return;
  }
  last_cookie_update_map_[domain] = base::Time::Now();
  cookie_requests_.push_back(CookieRequest::CreateAddCookieRequest(domain));
  ApplyCookieRequests();
}

bool AccountConsistencyService::ShouldSetChromeConnectedCookieToDomain(
    const std::string& domain,
    const base::TimeDelta& cookie_refresh_interval) {
  auto domain_iterator = last_cookie_update_map_.find(domain);
  bool domain_not_found = domain_iterator == last_cookie_update_map_.end();
  return domain_not_found || ((base::Time::Now() - domain_iterator->second) >
                              cookie_refresh_interval);
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
  applying_cookie_requests_ = true;

  const GURL url("https://" + cookie_requests_.front().domain);
  std::string cookie_value;
  base::Time expiration_date;
  switch (cookie_requests_.front().request_type) {
    case ADD_CHROME_CONNECTED_COOKIE:
      cookie_value = signin::BuildMirrorRequestCookieIfPossible(
          url, identity_manager_->GetPrimaryAccountInfo().gaia,
          signin::AccountConsistencyMethod::kMirror, cookie_settings_.get(),
          signin::PROFILE_MODE_DEFAULT);
      if (cookie_value.empty()) {
        // Don't add the cookie. Tentatively correct |last_cookie_update_map_|.
        last_cookie_update_map_.erase(cookie_requests_.front().domain);
        FinishedApplyingChromeConnectedCookieRequest(false);
        return;
      }
      // Create expiration date of Now+2y to roughly follow the SAPISID cookie.
      expiration_date = base::Time::Now() + base::TimeDelta::FromDays(730);
      break;
    case REMOVE_CHROME_CONNECTED_COOKIE:
      // Default values correspond to removing the cookie (no value, expiration
      // date in the past).
      expiration_date = base::Time::UnixEpoch();
      break;
  }

  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url,
          /*name=*/kChromeConnectedCookieName, cookie_value,
          /*domain=*/url.host(),
          /*path=*/std::string(),
          /*creation_time=*/base::Time::Now(), expiration_date,
          /*last_access_time=*/base::Time(),
          /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT);
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  cookie_manager->SetCanonicalCookie(
      *cookie, url, options,
      base::BindOnce(
          &AccountConsistencyService::FinishedSetChromeConnectedCookie,
          base::Unretained(this)));
}

void AccountConsistencyService::FinishedSetChromeConnectedCookie(
    net::CookieAccessResult cookie_access_result) {
  DCHECK(cookie_access_result.status.IsInclude());
  FinishedApplyingChromeConnectedCookieRequest(true);
}

void AccountConsistencyService::FinishedApplyingChromeConnectedCookieRequest(
    bool success) {
  // Do not process if the cookie requests are no longer available. This may
  // occur in a race condition on signout with data removed when cookie
  // requests are asynchronously processed but browsing data has been removed.
  // This fix is targeted for M86, crbug.com/1120450.
  if (cookie_requests_.empty()) {
    return;
  }
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
        update->RemoveKey(request.domain);
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

void AccountConsistencyService::AddChromeConnectedCookies() {
  DCHECK(!browser_state_->IsOffTheRecord());
  // These cookie request are preventive and not a strong signal (unlike
  // navigation to a domain). Don't force update the old cookies in this case.
  SetChromeConnectedCookieWithDomain(kGoogleDomain, base::TimeDelta::Max());
  SetChromeConnectedCookieWithDomain(kYoutubeDomain, base::TimeDelta::Max());
}

void AccountConsistencyService::OnBrowsingDataRemoved() {
  // CHROME_CONNECTED cookies have been removed, update internal state
  // accordingly.
  for (auto& cookie_request : cookie_requests_) {
    base::OnceClosure callback(std::move(cookie_request.callback));
    if (!callback.is_null()) {
      std::move(callback).Run();
    }
  }
  cookie_requests_.clear();
  last_cookie_update_map_.clear();
  last_gaia_cookie_verification_time_ = base::Time();
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
