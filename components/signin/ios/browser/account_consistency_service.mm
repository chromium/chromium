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
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "google_apis/gaia/gaia_constants.h"
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

// The Gaia cookie state on navigation for a signed-in Chrome user.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GaiaCookieStateOnSignedInNavigation {
  kGaiaCookiePresentOnNavigation = 0,
  kGaiaCookieAbsentOnGoogleAssociatedDomainNavigation = 1,
  kGaiaCookieAbsentOnAddSessionNavigation = 2,
  kGaiaCookieRestoredOnShowInfobar = 3,
  kMaxValue = kGaiaCookieRestoredOnShowInfobar
};

// Records the state of Gaia cookies for a navigation in UMA histogram.
void LogIOSGaiaCookiesState(GaiaCookieStateOnSignedInNavigation state) {
  base::UmaHistogramEnumeration("Signin.IOSGaiaCookieStateOnSignedInNavigation",
                                state);
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

}  // namespace

// WebStatePolicyDecider that monitors the HTTP headers on Gaia responses,
// reacting on the X-Chrome-Manage-Accounts header and notifying its delegate.
// It also notifies the AccountConsistencyService of domains it should add the
// CHROME_CONNECTED cookie to.
class AccountConsistencyService::AccountConsistencyHandler
    : public web::WebStatePolicyDecider,
      public web::WebStateObserver {
 public:
  AccountConsistencyHandler(web::WebState* web_state,
                            AccountConsistencyService* service,
                            AccountReconcilor* account_reconcilor,
                            signin::IdentityManager* identity_manager,
                            id<ManageAccountsDelegate> delegate);

  void WebStateDestroyed(web::WebState* web_state) override;

  AccountConsistencyHandler(const AccountConsistencyHandler&) = delete;
  AccountConsistencyHandler& operator=(const AccountConsistencyHandler&) =
      delete;

 private:
  // web::WebStateObserver override.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

  // web::WebStatePolicyDecider override.
  WebStatePolicyDecider::PolicyDecision ShouldAllowRequest(
      NSURLRequest* request,
      const web::WebStatePolicyDecider::RequestInfo& request_info) override;
  // Decides on navigation corresponding to |response| whether the navigation
  // should continue and updates authentication cookies on Google domains.
  void ShouldAllowResponse(
      NSURLResponse* response,
      bool for_main_frame,
      base::OnceCallback<void(PolicyDecision)> callback) override;
  void WebStateDestroyed() override;

  // Marks that GAIA cookies have been restored.
  void MarkGaiaCookiesRestored();

  // Loads |url| in the current tab.
  void NavigateToURL(GURL url);

  bool show_consistency_promo_ = false;
  bool gaia_cookies_restored_ = false;
  AccountConsistencyService* account_consistency_service_;  // Weak.
  AccountReconcilor* account_reconcilor_;                   // Weak.
  signin::IdentityManager* identity_manager_;
  web::WebState* web_state_;
  __weak id<ManageAccountsDelegate> delegate_;
  base::WeakPtrFactory<AccountConsistencyHandler> weak_ptr_factory_;
};

AccountConsistencyService::AccountConsistencyHandler::AccountConsistencyHandler(
    web::WebState* web_state,
    AccountConsistencyService* service,
    AccountReconcilor* account_reconcilor,
    signin::IdentityManager* identity_manager,
    id<ManageAccountsDelegate> delegate)
    : web::WebStatePolicyDecider(web_state),
      account_consistency_service_(service),
      account_reconcilor_(account_reconcilor),
      identity_manager_(identity_manager),
      web_state_(web_state),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  web_state->AddObserver(this);
}

web::WebStatePolicyDecider::PolicyDecision
AccountConsistencyService::AccountConsistencyHandler::ShouldAllowRequest(
    NSURLRequest* request,
    const web::WebStatePolicyDecider::RequestInfo& request_info) {
  GURL url = net::GURLWithNSURL(request.URL);
  if (base::FeatureList::IsEnabled(signin::kRestoreGaiaCookiesOnUserAction) &&
      signin::IsUrlEligibleForMirrorCookie(url) &&
      identity_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kNotRequired)) {
    // CHROME_CONNECTED cookies are added asynchronously on google.com and
    // youtube.com domains when Chrome detects that the user is signed-in. By
    // continuing to fulfill the navigation once the cookie request is sent,
    // Chrome adopts a best-effort strategy for signing the user into the web if
    // necessary.
    account_consistency_service_->AddChromeConnectedCookies();
  }
  return PolicyDecision::Allow();
}

void AccountConsistencyService::AccountConsistencyHandler::ShouldAllowResponse(
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
  }

  // Chrome monitors GAIA cookies when navigating to Google associated domains
  // to ensure that signed-in users remain signed-in to their Google services on
  // the web. This includes redirects to accounts.google.com.
  if (google_util::IsGoogleAssociatedDomainUrl(url)) {
    // TODO(crbug.com/1131027): Disable GAIA cookie restore on Google URLs that
    // may display cookie consent in the content area that conflict with the
    // sign-in notification. This will be removed once we perform cookie
    // restoration before sending a navigation request.
    if (!(google_util::IsGoogleHomePageUrl(url) ||
          google_util::IsGoogleSearchUrl(url))) {
      // Reset boolean that tracks displaying the sign-in notification infobar.
      // This ensures that only the most recent navigation will trigger an
      // infobar.
      gaia_cookies_restored_ = false;
      account_consistency_service_->SetGaiaCookiesIfDeleted(
          base::BindOnce(&AccountConsistencyHandler::MarkGaiaCookiesRestored,
                         weak_ptr_factory_.GetWeakPtr()));
    }
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
      // This situation is only possible if the all cookies have been deleted by
      // ITP restrictions and Chrome has not triggered a cookie refresh.
      if (identity_manager_->HasPrimaryAccount(
              signin::ConsentLevel::kNotRequired)) {
        LogIOSGaiaCookiesState(GaiaCookieStateOnSignedInNavigation::
                                   kGaiaCookieAbsentOnAddSessionNavigation);
        if (base::FeatureList::IsEnabled(
                signin::kRestoreGaiaCookiesOnUserAction)) {
          GURL continue_url = GURL(params.continue_url);
          DLOG_IF(ERROR,
                  !params.continue_url.empty() && !continue_url.is_valid())
              << "Invalid continuation URL: \"" << continue_url << "\"";
          if (account_consistency_service_->RestoreGaiaCookies(base::BindOnce(
                  &AccountConsistencyHandler::NavigateToURL,
                  weak_ptr_factory_.GetWeakPtr(), continue_url))) {
            // Continue URL will be processed in a callback once Gaia cookies
            // have been restored.
            return;
          }
        }
      }
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

void AccountConsistencyService::AccountConsistencyHandler::
    MarkGaiaCookiesRestored() {
  gaia_cookies_restored_ = true;
}

void AccountConsistencyService::AccountConsistencyHandler::NavigateToURL(
    GURL url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  [delegate_ onRestoreGaiaCookies];
  LogIOSGaiaCookiesState(
      GaiaCookieStateOnSignedInNavigation::kGaiaCookieRestoredOnShowInfobar);
}

void AccountConsistencyService::AccountConsistencyHandler::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  const GURL& url = web_state->GetLastCommittedURL();
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE ||
      !google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return;
  }

  // Displays the sign-in notification infobar if GAIA cookies have been
  // restored. This occurs once the URL has been loaded to avoid a race
  // condition in which the infobar is dismissed prior to the page load.
  if (gaia_cookies_restored_) {
    [delegate_ onRestoreGaiaCookies];
    LogIOSGaiaCookiesState(
        GaiaCookieStateOnSignedInNavigation::kGaiaCookieRestoredOnShowInfobar);
    gaia_cookies_restored_ = false;
  }

  if (show_consistency_promo_ && gaia::IsGaiaSignonRealm(url.GetOrigin())) {
    [delegate_ onShowConsistencyPromo];
    show_consistency_promo_ = false;

    // Chrome uses the CHROME_CONNECTED cookie to determine whether the
    // eligibility promo should be shown. Once it is shown we should remove the
    // cookie, since it should otherwise not be used unless the user is signed
    // in.
    account_consistency_service_->RemoveAllChromeConnectedCookies(
        base::OnceClosure());
  }
}

void AccountConsistencyService::AccountConsistencyHandler::WebStateDestroyed(
    web::WebState* web_state) {}

void AccountConsistencyService::AccountConsistencyHandler::WebStateDestroyed() {
  account_consistency_service_->RemoveWebStateHandler(web_state());
}

AccountConsistencyService::AccountConsistencyService(
    web::BrowserState* browser_state,
    AccountReconcilor* account_reconcilor,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    signin::IdentityManager* identity_manager)
    : browser_state_(browser_state),
      account_reconcilor_(account_reconcilor),
      cookie_settings_(cookie_settings),
      identity_manager_(identity_manager),
      active_cookie_manager_requests_for_testing_(0) {
  identity_manager_->AddObserver(this);
  if (identity_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kNotRequired)) {
    AddChromeConnectedCookies();
  } else {
    RemoveAllChromeConnectedCookies(base::OnceClosure());
  }
}

AccountConsistencyService::~AccountConsistencyService() {}

BOOL AccountConsistencyService::RestoreGaiaCookies(
    base::OnceClosure cookies_restored_callback) {
  // Only processes a single restoration attempt for a given amount of time to
  // avoid redirect loops.
  if (last_gaia_cookie_update_time_.is_null() ||
      base::Time::Now() - last_gaia_cookie_update_time_ <
          GetDelayThresholdToUpdateGaiaCookie()) {
    gaia_cookies_restored_callbacks_.push_back(
        std::move(cookies_restored_callback));
    identity_manager_->GetAccountsCookieMutator()->ForceTriggerOnCookieChange();
    last_gaia_cookie_update_time_ = base::Time::Now();
    return YES;
  }
  return NO;
}

void AccountConsistencyService::SetWebStateHandler(
    web::WebState* web_state,
    id<ManageAccountsDelegate> delegate) {
  DCHECK(!is_shutdown_) << "SetWebStateHandler called after Shutdown";
  DCHECK(handlers_map_.find(web_state) == handlers_map_.end());
  handlers_map_.insert(std::make_pair(
      web_state,
      std::make_unique<AccountConsistencyHandler>(
          web_state, this, account_reconcilor_, identity_manager_, delegate)));
}

void AccountConsistencyService::RemoveWebStateHandler(
    web::WebState* web_state) {
  DCHECK(!is_shutdown_) << "RemoveWebStateHandler called after Shutdown";
  auto iter = handlers_map_.find(web_state);
  DCHECK(iter != handlers_map_.end());

  std::unique_ptr<AccountConsistencyHandler> handler = std::move(iter->second);
  handlers_map_.erase(iter);

  web_state->RemoveObserver(handler.get());
}

void AccountConsistencyService::SetGaiaCookiesIfDeleted(
    base::OnceClosure cookies_restored_callback) {
  // We currently enforce a time threshold to update the Gaia cookie
  // for signed-in users to prevent calling the expensive method
  // |GetAllCookies| in the cookie manager.
  if (base::Time::Now() - last_gaia_cookie_verification_time_ <
          GetDelayThresholdToUpdateGaiaCookie() ||
      !identity_manager_->HasPrimaryAccount(
          signin::ConsentLevel::kNotRequired)) {
    return;
  }
  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();
  cookie_manager->GetCookieList(
      GaiaUrls::GetInstance()->secure_google_url(),
      net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(
          &AccountConsistencyService::TriggerGaiaCookieChangeIfDeleted,
          base::Unretained(this), std::move(cookies_restored_callback)));
  last_gaia_cookie_verification_time_ = base::Time::Now();
}

void AccountConsistencyService::TriggerGaiaCookieChangeIfDeleted(
    base::OnceClosure cookies_restored_callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& unused_excluded_cookies) {
  for (const auto& cookie : cookie_list) {
    if (cookie.cookie.Name() == GaiaConstants::kGaiaSigninCookieName) {
      LogIOSGaiaCookiesState(
          GaiaCookieStateOnSignedInNavigation::kGaiaCookiePresentOnNavigation);
      return;
    }
  }

  // The SAPISID cookie may have been deleted previous to this update due to
  // ITP restrictions marking Google domains as potential trackers.
  LogIOSGaiaCookiesState(
      GaiaCookieStateOnSignedInNavigation::
          kGaiaCookieAbsentOnGoogleAssociatedDomainNavigation);

  if (!base::FeatureList::IsEnabled(signin::kRestoreGaiaCookiesIfDeleted)) {
    return;
  }

  // Re-generate cookie to ensure that the user is properly signed in.
  identity_manager_->GetAccountsCookieMutator()->ForceTriggerOnCookieChange();
  gaia_cookies_restored_callbacks_.push_back(
      std::move(cookies_restored_callback));
}

void AccountConsistencyService::RemoveAllChromeConnectedCookies(
    base::OnceClosure callback) {
  DCHECK(!browser_state_->IsOffTheRecord());

  network::mojom::CookieManager* cookie_manager =
      browser_state_->GetCookieManager();

  network::mojom::CookieDeletionFilterPtr filter =
      network::mojom::CookieDeletionFilter::New();
  filter->cookie_name = signin::kChromeConnectedCookieName;

  ++active_cookie_manager_requests_for_testing_;
  cookie_manager->DeleteCookies(
      std::move(filter),
      base::BindOnce(&AccountConsistencyService::OnDeleteCookiesFinished,
                     base::Unretained(this), std::move(callback)));
  last_gaia_cookie_verification_time_ = base::Time();
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
  for (const GURL& url : urls) {
    SetChromeConnectedCookieWithUrl(url);
  }
}

void AccountConsistencyService::Shutdown() {
  DCHECK(handlers_map_.empty()) << "Handlers not unregistered at shutdown";
  identity_manager_->RemoveObserver(this);
  is_shutdown_ = true;
}

void AccountConsistencyService::SetChromeConnectedCookieWithUrl(
    const GURL& url) {
  const std::string domain = GetDomainFromUrl(url);
  std::string cookie_value = signin::BuildMirrorRequestCookieIfPossible(
      url,
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .gaia,
      signin::AccountConsistencyMethod::kMirror, cookie_settings_.get(),
      signin::PROFILE_MODE_DEFAULT);
  if (cookie_value.empty()) {
    return;
  }

  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url,
          /*name=*/signin::kChromeConnectedCookieName, cookie_value,
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
          net::COOKIE_PRIORITY_DEFAULT, /*same_party=*/false);
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
          base::Unretained(this)));
}

void AccountConsistencyService::OnChromeConnectedCookieFinished(
    net::CookieAccessResult cookie_access_result) {
  DCHECK(cookie_access_result.status.IsInclude());
  --active_cookie_manager_requests_for_testing_;
}

void AccountConsistencyService::AddChromeConnectedCookies() {
  DCHECK(!browser_state_->IsOffTheRecord());
  // These cookie requests are preventive. Chrome cannot be sure that
  // CHROME_CONNECTED cookies are set on google.com and youtube.com domains due
  // to ITP restrictions.
  SetChromeConnectedCookieWithUrls({GURL(kGoogleUrl), GURL(kYoutubeUrl)});
}

void AccountConsistencyService::OnBrowsingDataRemoved() {
  // CHROME_CONNECTED cookies have been removed, update internal state
  // accordingly.
  last_gaia_cookie_verification_time_ = base::Time();

  // SAPISID cookie has been removed, notify the GCMS.
  // TODO(https://crbug.com/930582) : Remove the need to expose this method
  // or move it to the network::CookieManager.
  identity_manager_->GetAccountsCookieMutator()->ForceTriggerOnCookieChange();
}

void AccountConsistencyService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kNotRequired)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      AddChromeConnectedCookies();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      RemoveAllChromeConnectedCookies(base::OnceClosure());
      break;
  }
}

void AccountConsistencyService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  AddChromeConnectedCookies();

  // If signed-in accounts have been recently restored through GAIA cookie
  // restoration then run the relevant callback to finish the update process.
  if (accounts_in_cookie_jar_info.signed_in_accounts.size() > 0 &&
      !gaia_cookies_restored_callbacks_.empty()) {
    std::vector<base::OnceClosure> callbacks;
    std::swap(gaia_cookies_restored_callbacks_, callbacks);
    for (base::OnceClosure& callback : callbacks) {
      std::move(callback).Run();
    }
  }
}
