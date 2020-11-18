// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
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

const char* kGoogleUrl = "https://google.com";
const char* kYoutubeUrl = "https://youtube.com";
const char* kGaiaDomain = "accounts.google.com";

// Returns the registered, organization-identifying host, but no subdomains,
// from the given GURL. Returns an empty string if the GURL is invalid.
static std::string GetDomainFromUrl(const GURL& url) {
  if (gaia::IsGaiaSignonRealm(url.GetOrigin())) {
    return kGaiaDomain;
  }
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

// Allows for manual testing by reducing the polling interval for verifying the
// existence of the GAIA cookie.
base::TimeDelta GetDelayThresholdToUpdateGaiaCookie() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          signin::kDelayThresholdMinutesToUpdateGaiaCookie)) {
    std::string delayString = command_line->GetSwitchValueASCII(
        signin::kDelayThresholdMinutesToUpdateGaiaCookie);
    int commandLineDelay = 0;
    if (base::StringToInt(delayString, &commandLineDelay)) {
      return base::TimeDelta::FromMinutes(commandLineDelay);
    }
  }
  return kDelayThresholdToUpdateGaiaCookie;
}

// WebStatePolicyDecider that monitors the HTTP headers on Gaia responses,
// reacting on the X-Chrome-Manage-Accounts header and notifying its delegate.
// It also notifies the AccountConsistencyService of domains it should add the
// CHROME_CONNECTED cookie to.
class AccountConsistencyHandler : public web::WebStatePolicyDecider,
                                  public web::WebStateObserver {
 public:
  AccountConsistencyHandler(web::WebState* web_state,
                            AccountConsistencyService* service,
                            AccountReconcilor* account_reconcilor,
                            id<ManageAccountsDelegate> delegate);

  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // web::WebStateObserver override.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

  // web::WebStatePolicyDecider override.
  // Decides on navigation corresponding to |response| whether the navigation
  // should continue and updates authentication cookies on Google domains.
  void ShouldAllowResponse(
      NSURLResponse* response,
      bool for_main_frame,
      base::OnceCallback<void(PolicyDecision)> callback) override;
  void WebStateDestroyed() override;

  bool show_consistency_promo_ = false;
  AccountConsistencyService* account_consistency_service_;  // Weak.
  AccountReconcilor* account_reconcilor_;                   // Weak.
  __weak id<ManageAccountsDelegate> delegate_;
};
}  // namespace

AccountConsistencyHandler::AccountConsistencyHandler(
    web::WebState* web_state,
    AccountConsistencyService* service,
    AccountReconcilor* account_reconcilor,
    id<ManageAccountsDelegate> delegate)
    : web::WebStatePolicyDecider(web_state),
      account_consistency_service_(service),
      account_reconcilor_(account_reconcilor),
      delegate_(delegate) {
  web_state->AddObserver(this);
}

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
  // User is showing intent to navigate to a Google-owned domain. Set GAIA and
  // CHROME_CONNECTED cookies if the user is signed in or if they are not signed
  // in and navigating to a GAIA sign-on (this is filtered in
  // ChromeConnectedHelper).
  if (signin::IsUrlEligibleForMirrorCookie(url)) {
    account_consistency_service_->SetChromeConnectedCookieWithUrls(
        {url, GURL(kGoogleUrl)});
    account_consistency_service_->SetGaiaCookiesIfDeleted();
  }

  if (!gaia::IsGaiaSignonRealm(url.GetOrigin())) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }
  NSString* manage_accounts_header = [[http_response allHeaderFields]
      objectForKey:
          [NSString stringWithUTF8String:signin::kChromeManageAccountsHeader]];
  if (!manage_accounts_header) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  signin::ManageAccountsParams params = signin::BuildManageAccountsParams(
      base::SysNSStringToUTF8(manage_accounts_header));

  account_reconcilor_->OnReceivedManageAccountsResponse(params.service_type);
  // Reset boolean that tracks displaying the sign-in consistency promo. This
  // ensures that the promo is cancelled once navigation has started and the
  // WKWebView is cancelling previous navigations.
  show_consistency_promo_ = false;

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
      if (params.show_consistency_promo) {
        show_consistency_promo_ = true;
        // Allows the URL response to load before showing the consistency promo.
        // The promo should always be displayed in the foreground of Gaia
        // sign-on.
        std::move(callback).Run(PolicyDecision::Allow());
        return;
      } else {
        [delegate_ onAddAccount];
      }
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

void AccountConsistencyHandler::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (!show_consistency_promo_ ||
      !gaia::IsGaiaSignonRealm(web_state->GetLastCommittedURL().GetOrigin()) ||
      load_completion_status == web::PageLoadCompletionStatus::FAILURE) {
    return;
  }
  [delegate_ onShowConsistencyPromo];
  show_consistency_promo_ = false;

  // Chrome uses the CHROME_CONNECTED cookie to determine whether the
  // eligibility promo should be shown. Once it is shown we should remove the
  // cookie, since it should otherwise not be used unless the user is signed in.
  account_consistency_service_->RemoveAllChromeConnectedCookies(
      base::OnceClosure());
}

void AccountConsistencyHandler::WebStateDestroyed(web::WebState* web_state) {}

void AccountConsistencyHandler::WebStateDestroyed() {
  account_consistency_service_->RemoveWebStateHandler(web_state());
}

const char AccountConsistencyService::kChromeConnectedCookieName[] =
    "CHROME_CONNECTED";

const char AccountConsistencyService::kGaiaCookieName[] = "SAPISID";

const char AccountConsistencyService::kDomainsWithCookiePref[] =
    "signin.domains_with_cookie";

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
      active_cookie_manager_requests_for_testing_(0) {
  identity_manager_->AddObserver(this);
  LoadFromPrefs();
  if (identity_manager_->HasPrimaryAccount()) {
    AddChromeConnectedCookies();
  } else {
    RemoveAllChromeConnectedCookies(base::OnceClosure());
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
  web_state->RemoveObserver(
      (AccountConsistencyHandler*)web_state_handlers_[web_state].get());
  web_state_handlers_.erase(web_state);
}

void AccountConsistencyService::SetGaiaCookiesIfDeleted() {
  // We currently enforce a time threshold to update the Gaia cookie
  // for signed-in users to prevent calling the expensive method
  // |GetAllCookies| in the cookie manager.
  if (base::Time::Now() - last_gaia_cookie_verification_time_ <
          GetDelayThresholdToUpdateGaiaCookie() ||
      !identity_manager_->HasPrimaryAccount()) {
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

  if (!base::FeatureList::IsEnabled(signin::kRestoreGaiaCookiesIfDeleted)) {
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

void AccountConsistencyService::RemoveAllChromeConnectedCookies(
    base::OnceClosure callback) {
  DCHECK(!browser_state_->IsOffTheRecord());
  if (last_cookie_update_map_.empty()) {
    if (!callback.is_null())
      std::move(callback).Run();
    return;
  }

  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();

  network::mojom::CookieDeletionFilterPtr filter =
      network::mojom::CookieDeletionFilter::New();
  filter->cookie_name = kChromeConnectedCookieName;

  ++active_cookie_manager_requests_for_testing_;
  cookie_manager->DeleteCookies(
      std::move(filter),
      base::BindOnce(&AccountConsistencyService::OnDeleteCookiesFinished,
                     base::Unretained(this), std::move(callback)));
  ResetInternalState();
}

void AccountConsistencyService::OnDeleteCookiesFinished(
    base::OnceClosure callback,
    uint32_t unused_num_cookies_deleted) {
  --active_cookie_manager_requests_for_testing_;
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void AccountConsistencyService::SetChromeConnectedCookieWithUrls(
    const std::vector<const GURL>& urls) {
  SetChromeConnectedCookieWithUrls(
      urls, kDelayThresholdToUpdateChromeConnectedCookie);
}

void AccountConsistencyService::SetChromeConnectedCookieWithUrls(
    const std::vector<const GURL>& urls,
    const base::TimeDelta& cookie_refresh_interval) {
  for (const GURL& url : urls) {
    const std::string domain = GetDomainFromUrl(url);
    if (!ShouldSetChromeConnectedCookieToDomain(domain,
                                                cookie_refresh_interval)) {
      continue;
    }
    last_cookie_update_map_[domain] = base::Time::Now();
    SetChromeConnectedCookieWithUrl(url);
  }
}

bool AccountConsistencyService::ShouldSetChromeConnectedCookieToDomain(
    const std::string& domain,
    const base::TimeDelta& cookie_refresh_interval) {
  auto domain_iterator = last_cookie_update_map_.find(domain);
  bool domain_not_found = domain_iterator == last_cookie_update_map_.end();
  return domain_not_found || ((base::Time::Now() - domain_iterator->second) >
                              cookie_refresh_interval);
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

void AccountConsistencyService::SetChromeConnectedCookieWithUrl(
    const GURL& url) {
  const std::string domain = GetDomainFromUrl(url);
  std::string cookie_value = signin::BuildMirrorRequestCookieIfPossible(
      url, identity_manager_->GetPrimaryAccountInfo().gaia,
      signin::AccountConsistencyMethod::kMirror, cookie_settings_.get(),
      signin::PROFILE_MODE_DEFAULT);
  if (cookie_value.empty()) {
    last_cookie_update_map_.erase(domain);
    return;
  }

  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url,
          /*name=*/kChromeConnectedCookieName, cookie_value,
          /*domain=*/domain,
          /*path=*/std::string(),
          /*creation_time=*/base::Time::Now(),
          // Create expiration date of Now+2y to roughly follow the SAPISID
          // cookie.
          /*expiration_time=*/base::Time::Now() +
              base::TimeDelta::FromDays(730),
          /*last_access_time=*/base::Time(),
          /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT);
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  ++active_cookie_manager_requests_for_testing_;

  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  cookie_manager->SetCanonicalCookie(
      *cookie, url, options,
      base::BindOnce(
          &AccountConsistencyService::OnChromeConnectedCookieFinished,
          base::Unretained(this), domain));
}

void AccountConsistencyService::OnChromeConnectedCookieFinished(
    const std::string& domain,
    net::CookieAccessResult cookie_access_result) {
  DCHECK(cookie_access_result.status.IsInclude());
  DictionaryPrefUpdate update(
      prefs_, AccountConsistencyService::kDomainsWithCookiePref);
  // Add request.domain to prefs, use |true| as a dummy value (that is
  // never used), as the dictionary is used as a set.
  update->SetKey(domain, base::Value(true));
  --active_cookie_manager_requests_for_testing_;
}

void AccountConsistencyService::AddChromeConnectedCookies() {
  DCHECK(!browser_state_->IsOffTheRecord());
  // These cookie requests are preventive and not a strong signal (unlike
  // navigation to a domain). Don't force update the old cookies in this case.
  SetChromeConnectedCookieWithUrls({GURL(kGoogleUrl), GURL(kYoutubeUrl)},
                                   base::TimeDelta::Max());
}

void AccountConsistencyService::ResetInternalState() {
  last_cookie_update_map_.clear();
  last_gaia_cookie_verification_time_ = base::Time();
  base::DictionaryValue dict;
  prefs_->Set(kDomainsWithCookiePref, dict);
}

void AccountConsistencyService::OnBrowsingDataRemoved() {
  // CHROME_CONNECTED cookies have been removed, update internal state
  // accordingly.
  ResetInternalState();

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
  RemoveAllChromeConnectedCookies(base::OnceClosure());
}

void AccountConsistencyService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  AddChromeConnectedCookies();
}
